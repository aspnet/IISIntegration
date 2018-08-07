// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "FileOutputManager.h"
#include "sttimer.h"
#include "utility.h"
#include "exceptions.h"
#include "debugutil.h"
#include "SRWExclusiveLock.h"
#include "PipeWrapper.h"

extern HINSTANCE    g_hModule;

FileOutputManager::FileOutputManager() :
    m_hLogFileHandle(INVALID_HANDLE_VALUE),
    m_fdPreviousStdOut(-1),
    m_fdPreviousStdErr(-1),
    m_disposed(false),
    stdoutWrapper(nullptr),
    stderrWrapper(nullptr)
{
    InitializeSRWLock(&m_srwLock);
}

FileOutputManager::~FileOutputManager()
{
    Stop();

    CloseHandle(m_hLogFileHandle);
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
    CHAR            pzFileContents[MAX_FILE_READ_SIZE] = { 0 };
    DWORD           dwNumBytesRead;
    LARGE_INTEGER   li = { 0 };
    BOOL            fLogged = FALSE;
    DWORD           dwFilePointer = 0;

    if (m_hLogFileHandle != INVALID_HANDLE_VALUE)
    {
        if (GetFileSizeEx(m_hLogFileHandle, &li) && li.LowPart > 0 && li.HighPart == 0)
        {
            dwFilePointer = SetFilePointer(m_hLogFileHandle, 0, NULL, FILE_BEGIN);

            if (dwFilePointer != INVALID_SET_FILE_POINTER)
            {
                // Read file fails.
                if (ReadFile(m_hLogFileHandle, pzFileContents, MAX_FILE_READ_SIZE, &dwNumBytesRead, NULL))
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

    WCHAR path[MAX_PATH];
    RETURN_LAST_ERROR_IF(!GetModuleFileName(g_hModule, path, sizeof(path)));
    std::filesystem::path fsPath(path);

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
            fsPath.filename().stem().c_str()));

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

    stderrWrapper = std::make_unique<PipeWrapper>(stderr, STD_ERROR_HANDLE, m_hLogFileHandle, GetStdHandle(STD_ERROR_HANDLE));
    stdoutWrapper = std::make_unique<PipeWrapper>(stdout, STD_OUTPUT_HANDLE, m_hLogFileHandle, GetStdHandle(STD_OUTPUT_HANDLE));

    RETURN_IF_FAILED(stderrWrapper->SetupRedirection());
    RETURN_IF_FAILED(stdoutWrapper->SetupRedirection());

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


    // Periodically flush the log content to file
    m_Timer.InitializeTimer(STTIMER::TimerCallback, &m_struLogFilePath, 3000, 3000);

    return S_OK;
}


HRESULT
FileOutputManager::Stop()
{
    STRA     straStdOutput;

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
        FlushFileBuffers(m_hLogFileHandle);
    }

    if (stdoutWrapper != nullptr)
    {
        RETURN_IF_FAILED(stdoutWrapper->StopRedirection());
    }

    if (stderrWrapper != nullptr)
    {
        RETURN_IF_FAILED(stderrWrapper->StopRedirection());
    }

    // delete empty log file
    handle = FindFirstFile(m_struLogFilePath.QueryStr(), &fileData);
    if (handle != INVALID_HANDLE_VALUE &&
        handle != NULL &&
        fileData.nFileSizeHigh == 0 &&
        fileData.nFileSizeLow == 0) // skip check of nFileSizeHigh
    {
        FindClose(handle);
        LOG_LAST_ERROR_IF(!DeleteFile(m_struLogFilePath.QueryStr()));
    }

    // If we captured any output, relog it to the original stdout
    // Useful for the IIS Express scenario as it is running with stdout and stderr

    if (GetStdOutContent(&straStdOutput))
    {
        int res = printf(straStdOutput.QueryStr());
        // This will fail on full IIS (which is fine).
        RETURN_LAST_ERROR_IF(res == -1);

        // Need to flush contents for the new stdout and stderr
        _flushall();
    }

    return S_OK;
}
