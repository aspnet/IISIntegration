// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

PipeOutputManager::PipeOutputManager() :
    m_dwStdErrReadTotal(0),
    m_hLogFileHandle(INVALID_HANDLE_VALUE),
    m_hErrReadPipe(INVALID_HANDLE_VALUE),
    m_hErrWritePipe(INVALID_HANDLE_VALUE),
    m_hErrThread(INVALID_HANDLE_VALUE),
    m_fDisposed(FALSE)
{
}

PipeOutputManager::~PipeOutputManager()
{
    Dispose();
}

VOID
PipeOutputManager::Dispose()
{
    DWORD    dwThreadStatus = 0;
    STRU     struStdOutput;

    if (m_fDisposed)
    {
        return;
    }
    m_fDisposed = true;

    fflush(stdout);
    fflush(stderr);

    if (m_hErrWritePipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hErrWritePipe);
        m_hErrWritePipe = INVALID_HANDLE_VALUE;
    }

    if (m_hErrThread != NULL &&
        GetExitCodeThread(m_hErrThread, &dwThreadStatus) != 0 &&
        dwThreadStatus == STILL_ACTIVE)
    {
        // wait for gracefullshut down, i.e., the exit of the background thread or timeout
        if (WaitForSingleObject(m_hErrThread, 2000) != WAIT_OBJECT_0)
        {
            // if the thread is still running, we need kill it first before exit to avoid AV
            if (GetExitCodeThread(m_hErrThread, &dwThreadStatus) != 0 && dwThreadStatus == STILL_ACTIVE)
            {
                TerminateThread(m_hErrThread, STATUS_CONTROL_C_EXIT);
            }
        }
    }

    CloseHandle(m_hErrThread);
    m_hErrThread = NULL;

    if (m_hErrReadPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hErrReadPipe);
        m_hErrReadPipe = INVALID_HANDLE_VALUE;
    }

    _dup2(m_fdStdOut, 1);
    _dup2(m_fdStdErr, 2);

    // Write the remaining contents to the original stdout
    if (GetStdOutContent(&struStdOutput))
    {
        wprintf(struStdOutput.QueryStr());
        // Need to flush contents.
        _flushall();
    }
}

HRESULT PipeOutputManager::Start()
{
    HRESULT hr = S_OK;
    SECURITY_ATTRIBUTES     saAttr = { 0 };
    HANDLE                  hStdErrReadPipe;
    HANDLE                  hStdErrWritePipe;

    m_fdStdOut = _dup(1);
    m_fdStdErr = _dup(2);

    //
    // CreatePipe for outputting stderr to the windows event log.
    // Ignore failures
    //
    if (!CreatePipe(&hStdErrReadPipe, &hStdErrWritePipe, &saAttr, 0 /*nSize*/))
    {
        goto Finished;
    }

    // TODO this still doesn't redirect calls in native, like wprintf
    if (!SetStdHandle(STD_ERROR_HANDLE, hStdErrWritePipe))
    {
        goto Finished;
    }

    if (!SetStdHandle(STD_OUTPUT_HANDLE, hStdErrWritePipe))
    {
        goto Finished;
    }

    m_hErrReadPipe = hStdErrReadPipe;
    m_hErrWritePipe = hStdErrWritePipe;

    // Read the stderr handle on a separate thread until we get 4096 bytes.
    m_hErrThread = CreateThread(
        NULL,       // default security attributes
        0,          // default stack size
        (LPTHREAD_START_ROUTINE)ReadStdErrHandle,
        this,       // thread function arguments
        0,          // default creation flags
        NULL);      // receive thread identifier

    if (m_hErrThread == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Finished;
    }

Finished:
    return hr;
}

VOID
PipeOutputManager::ReadStdErrHandle(
    LPVOID pContext
)
{
    PipeOutputManager *pLoggingProvider = (PipeOutputManager*)pContext;
    DBG_ASSERT(pLoggingProvider != NULL);
    pLoggingProvider->ReadStdErrHandleInternal();
}

bool PipeOutputManager::GetStdOutContent(STRU* struStdOutput)
{
    bool fLogged = false;
    if (m_dwStdErrReadTotal > 0)
    {
        if (SUCCEEDED(struStdOutput->CopyA(m_pzFileContents, m_dwStdErrReadTotal)))
        {
            fLogged = TRUE;
        }
    }
    return fLogged;
}

VOID
PipeOutputManager::ReadStdErrHandleInternal(
    VOID
)
{
    DWORD dwNumBytesRead = 0;
    while (true)
    {
        if (ReadFile(m_hErrReadPipe, &m_pzFileContents[m_dwStdErrReadTotal], 4096 - m_dwStdErrReadTotal, &dwNumBytesRead, NULL))
        {
            m_dwStdErrReadTotal += dwNumBytesRead;
            if (m_dwStdErrReadTotal >= 4096)
            {
                break;
            }
        }
        else if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            break;
        }
    }
}

VOID
PipeOutputManager::NotifyStartupComplete()
{
    Dispose();
}
