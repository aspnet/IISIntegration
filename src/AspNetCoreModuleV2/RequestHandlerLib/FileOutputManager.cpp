// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "FileOutputManager.h"
#include "sttimer.h"
#include "utility.h"
#include "exceptions.h"
#include "debugutil.h"
#include "SRWExclusiveLock.h"
#include "LoggingHelpers.h"

extern PCWSTR              g_moduleName;

#define LOG_IF_ERRNO(err) do { if (err != 0) { LOG_IF_FAILED(HRESULT_FROM_WIN32(_doserrno)); } } while (0, 0);

FileOutputManager::FileOutputManager() :
    m_hLogFileHandle(INVALID_HANDLE_VALUE),
    m_fdPreviousStdOut(-1),
    m_fdPreviousStdErr(-1),
    m_disposed(false)
{
    InitializeSRWLock(&m_srwLock);
}

FileOutputManager::~FileOutputManager()
{
    Stop();
}

HRESULT
FileOutputManager::Initialize(PCWSTR pwzStdOutLogFileName, PCWSTR pwzApplicationPath)
{
    RETURN_IF_FAILED(m_wsApplicationPath.Copy(pwzApplicationPath));
    RETURN_IF_FAILED(m_wsStdOutLogFileName.Copy(pwzStdOutLogFileName));

    return S_OK;
}

bool FileOutputManager::GetStdOutContent(STRA* struStdOutput)
{
    //
    // Ungraceful shutdown, try to log an error message.
    // This will be a common place for errors as it means the hostfxr_main returned
    // or there was an exception.
    //
    CHAR            pzFileContents[4096] = { 0 };
    DWORD           dwNumBytesRead;
    LARGE_INTEGER   li = { 0 };
    BOOL            fLogged = FALSE;
    DWORD           dwFilePointer = 0;

    if (m_hLogFileHandle != INVALID_HANDLE_VALUE)
    {
        if (GetFileSizeEx(m_hLogFileHandle, &li) && li.LowPart > 0 && li.HighPart == 0)
        {
            if (li.LowPart > 4096)
            {
                dwFilePointer = SetFilePointer(m_hLogFileHandle, -4096, NULL, FILE_END);
            }
            else
            {
                dwFilePointer = SetFilePointer(m_hLogFileHandle, 0, NULL, FILE_BEGIN);
            }

            if (dwFilePointer != INVALID_SET_FILE_POINTER)
            {
                // Read file fails.
                if (ReadFile(m_hLogFileHandle, pzFileContents, 4096, &dwNumBytesRead, NULL))
                {
                    if (SUCCEEDED(struStdOutput->Copy(pzFileContents, dwNumBytesRead)))
                    {
                        fLogged = TRUE;
                    }
                }
            }
        }
    }

    return fLogged;
}

HRESULT
FileOutputManager::Start()
{
    SYSTEMTIME systemTime;
    SECURITY_ATTRIBUTES saAttr = { 0 };
    STRU struPath;

    RETURN_IF_FAILED(UTILITY::ConvertPathToFullPath(
        m_wsStdOutLogFileName.QueryStr(),
        m_wsApplicationPath.QueryStr(),
        &struPath));

    RETURN_IF_FAILED(UTILITY::EnsureDirectoryPathExist(struPath.QueryStr()));

    GetSystemTime(&systemTime);

    RETURN_IF_FAILED(
        m_struLogFilePath.SafeSnwprintf(L"%s_%d%02d%02d%02d%02d%02d_%d_%s.log",
        struPath.QueryStr(),
        systemTime.wYear,
        systemTime.wMonth,
        systemTime.wDay,
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond,
        GetCurrentProcessId(),
            g_moduleName));

    m_fdPreviousStdOut = _dup(_fileno(stdout));
    m_fdPreviousStdErr = _dup(_fileno(stderr));

    m_hLogFileHandle = CreateFileW(m_struLogFilePath.QueryStr(),
        FILE_READ_DATA | FILE_WRITE_DATA,
        FILE_SHARE_READ,
        &saAttr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (m_hLogFileHandle == INVALID_HANDLE_VALUE)
    {
        return LOG_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
    }

    // There are a few options for redirecting stdout/stderr,
    // but there are issues with most of them.
    // AllocConsole()
    // *stdout = *m_pStdFile;
    // *stderr = *m_pStdFile;
    // Calling _dup2 on stderr fails on IIS. IIS sets stderr to -2
    // _dup2(_fileno(m_pStdFile), _fileno(stdout));
    // _dup2(_fileno(m_pStdFile), _fileno(stderr));
    // If we were okay setting stdout and stderr to separate files, we could use:
    // _wfreopen_s(&m_pStdFile, struLogFileName.QueryStr(), L"w+", stdout);
    // _wfreopen_s(&m_pStdFile, struLogFileName.QueryStr(), L"w+", stderr);
    // Calling SetStdHandle works for redirecting managed logs, however you cannot
    // capture native logs (including hostfxr failures).

    RETURN_LAST_ERROR_IF(!SetStdHandle(STD_OUTPUT_HANDLE, m_hLogFileHandle));

    RETURN_LAST_ERROR_IF(!SetStdHandle(STD_ERROR_HANDLE, m_hLogFileHandle));

    LoggingHelpers::ReReadStdOutFileNo();
    LoggingHelpers::ReReadStdErrFileNo();

    // Periodically flush the log content to file
    m_Timer.InitializeTimer(STTIMER::TimerCallback, &m_struLogFilePath, 3000, 3000);

    LOG_INFOF("Created log file for inprocess application: %S",
        m_struLogFilePath.QueryStr());

    return S_OK;
}

HRESULT
FileOutputManager::Stop()
{
    if (m_disposed)
    {
        return S_OK;
    }
    SRWExclusiveLock lock(m_srwLock);
    if (m_disposed)
    {
        return S_OK;
    }
    m_disposed = true;

    HANDLE   handle = NULL;
    WIN32_FIND_DATA fileData;

    if (m_hLogFileHandle != INVALID_HANDLE_VALUE)
    {
        m_Timer.CancelTimer();
    }
    auto test = FlushFileBuffers(m_hLogFileHandle);
    if (test)
    {
        // std err has twice as much stuff
        LOG_INFO("Flushed buffers.");
    }

    handle = GetStdHandle(STD_ERROR_HANDLE);
    CloseHandle(handle);

    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CloseHandle(handle);

    if (m_fdPreviousStdOut >= 0)
    {
        LOG_LAST_ERROR_IF(!SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(m_fdPreviousStdOut)));
    }
    else
    {
        //LOG_IF_ERRNO(freopen_s(&stream, "NUL:", "w", stdout));
        //SetStdHandle(STD_OUTPUT_HANDLE, stream);
    }

    if (m_fdPreviousStdErr >= 0)
    {
        LOG_LAST_ERROR_IF(!SetStdHandle(STD_ERROR_HANDLE, (HANDLE)_get_osfhandle(m_fdPreviousStdErr)));
    }
    else
    {
        //LOG_IF_ERRNO(freopen_s(&stream, "NUL:", "w", stderr));
        //SetStdHandle(STD_OUTPUT_HANDLE, stream);
    }
    LoggingHelpers::ReReadStdOutFileNo();
    LoggingHelpers::ReReadStdErrFileNo();


    handle = FindFirstFile(m_struLogFilePath.QueryStr(), &fileData);
    if (handle != INVALID_HANDLE_VALUE &&
        handle != NULL &&
        fileData.nFileSizeHigh == 0 &&
        fileData.nFileSizeLow == 0) // skip check of nFileSizeHigh
    {
        LOG_INFO("Log is empty.");
        FindClose(handle);
        auto res = DeleteFile(m_struLogFilePath.QueryStr());
        LOG_LAST_ERROR_IF(!res);
    }

    m_hLogFileHandle = INVALID_HANDLE_VALUE;

    return S_OK;
}
