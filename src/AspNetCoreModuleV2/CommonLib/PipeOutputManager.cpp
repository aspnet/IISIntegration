// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "PipeOutputManager.h"
#include "exceptions.h"
#include "SRWExclusiveLock.h"
#include "PipeWrapper.h"
#include "ntassert.h"

#define LOG_IF_DUPFAIL(err) do { if (err == -1) { LOG_IF_FAILED(HRESULT_FROM_WIN32(_doserrno)); } } while (0, 0);
#define LOG_IF_ERRNO(err) do { if (err != 0) { LOG_IF_FAILED(HRESULT_FROM_WIN32(_doserrno)); } } while (0, 0);

PipeOutputManager::PipeOutputManager() :
    m_hErrReadPipe(INVALID_HANDLE_VALUE),
    m_hErrWritePipe(INVALID_HANDLE_VALUE),
    m_hErrThread(nullptr),
    m_dwStdErrReadTotal(0),
    m_disposed(FALSE),
    stdoutWrapper(nullptr),
    stderrWrapper(nullptr)
{
    InitializeSRWLock(&m_srwLock);
}

PipeOutputManager::~PipeOutputManager()
{
    PipeOutputManager::Stop();
}

HRESULT PipeOutputManager::Start()
{
    SECURITY_ATTRIBUTES     saAttr = { 0 };
    HANDLE                  hStdErrReadPipe;
    HANDLE                  hStdErrWritePipe;

    RETURN_LAST_ERROR_IF(!CreatePipe(&hStdErrReadPipe, &hStdErrWritePipe, &saAttr, 0 /*nSize*/));

    m_hErrReadPipe = hStdErrReadPipe;
    m_hErrWritePipe = hStdErrWritePipe;

    stderrWrapper = std::make_unique<PipeWrapper>(stderr, STD_ERROR_HANDLE, hStdErrWritePipe, GetStdHandle(STD_ERROR_HANDLE));
    stdoutWrapper = std::make_unique<PipeWrapper>(stdout, STD_OUTPUT_HANDLE, hStdErrWritePipe, GetStdHandle(STD_OUTPUT_HANDLE));

    RETURN_IF_FAILED(stderrWrapper->SetupRedirection());
    RETURN_IF_FAILED(stdoutWrapper->SetupRedirection());

    // Read the stderr handle on a separate thread until we get 4096 bytes.
    m_hErrThread = CreateThread(
        nullptr,       // default security attributes
        0,          // default stack size
        reinterpret_cast<LPTHREAD_START_ROUTINE>(ReadStdErrHandle),
        this,       // thread function arguments
        0,          // default creation flags
        nullptr);      // receive thread identifier

    RETURN_LAST_ERROR_IF_NULL(m_hErrThread);

    return S_OK;
}

HRESULT PipeOutputManager::Stop()
{
    DWORD    dwThreadStatus = 0;
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

    // Flush the pipe writer before closing to capture all output

    // Tell each pipe wrapper to stop redirecting output and restore the original values


    // Both pipe wrappers duplicate the pipe writer handle
    // meaning we are fine to close the handle too.
    if (m_hErrWritePipe != INVALID_HANDLE_VALUE)
    {
        RETURN_LAST_ERROR_IF(!FlushFileBuffers(m_hErrWritePipe));
        CloseHandle(m_hErrWritePipe);
        m_hErrWritePipe = INVALID_HANDLE_VALUE;
    }
    if (stdoutWrapper != nullptr)
    {
        RETURN_IF_FAILED(stdoutWrapper->StopRedirection());
        RETURN_IF_FAILED(stderrWrapper->StopRedirection());
    }

    // Forces ReadFile to cancel, causing the read loop to complete.
    // Don't check return value as IO may or may not be completed already.

     // GetExitCodeThread returns 0 on failure; thread status code is invalid.
    if (m_hErrThread != nullptr &&
        !LOG_LAST_ERROR_IF(GetExitCodeThread(m_hErrThread, &dwThreadStatus) == 0) &&
        dwThreadStatus == STILL_ACTIVE)
    {
        CancelSynchronousIo(m_hErrThread);
        // wait for graceful shutdown, i.e., the exit of the background thread or timeout
        if (WaitForSingleObject(m_hErrThread, PIPE_OUTPUT_THREAD_TIMEOUT) != WAIT_OBJECT_0)
        {
            // if the thread is still running, we need kill it first before exit to avoid AV
            if (!LOG_LAST_ERROR_IF(GetExitCodeThread(m_hErrThread, &dwThreadStatus) == 0) &&
                dwThreadStatus == STILL_ACTIVE)
            {
                LOG_WARN("Thread reading stdout/err hit timeout, forcibly closing thread.");
                TerminateThread(m_hErrThread, STATUS_CONTROL_C_EXIT);
            }
        }
    }

    if (m_hErrThread != nullptr)
    {
        CloseHandle(m_hErrThread);
        m_hErrThread = nullptr;
    }

    if (m_hErrReadPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hErrReadPipe);
        m_hErrReadPipe = INVALID_HANDLE_VALUE;
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

void
PipeOutputManager::ReadStdErrHandle(
    LPVOID pContext
)
{
    auto pLoggingProvider = static_cast<PipeOutputManager*>(pContext);
    DBG_ASSERT(pLoggingProvider != NULL);
    pLoggingProvider->ReadStdErrHandleInternal();
}

bool PipeOutputManager::GetStdOutContent(STRA* straStdOutput)
{
    bool fLogged = false;
    if (m_dwStdErrReadTotal > 0)
    {
        if (SUCCEEDED(straStdOutput->Copy(m_pzFileContents, m_dwStdErrReadTotal)))
        {
            fLogged = TRUE;
        }
    }

    return fLogged;
}

void
PipeOutputManager::ReadStdErrHandleInternal()
{
    // If ReadFile ever returns false, exit the thread

    DWORD dwNumBytesRead = 0;
    while (true)
    {
        // Fill a maximum of MAX_PIPE_READ_SIZE into a buffer.
        if (ReadFile(m_hErrReadPipe,
            &m_pzFileContents[m_dwStdErrReadTotal],
            MAX_PIPE_READ_SIZE - m_dwStdErrReadTotal,
            &dwNumBytesRead,
            nullptr))
        {
            m_dwStdErrReadTotal += dwNumBytesRead;
            if (m_dwStdErrReadTotal >= MAX_PIPE_READ_SIZE)
            {
                break;
            }
        }
        else
        {
            return;
        }
    }

    std::string tempBuffer; // Using std::string as a wrapper around new char[] so we don't need to call delete
    tempBuffer.resize(MAX_PIPE_READ_SIZE);

    // After reading the maximum amount of data, keep reading in a loop until Stop is called on the output manager.
    while (true)
    {
        if (ReadFile(m_hErrReadPipe, &tempBuffer[0], MAX_PIPE_READ_SIZE, &dwNumBytesRead, nullptr))
        {
        }
        else
        {
            return;
        }
    }
}
