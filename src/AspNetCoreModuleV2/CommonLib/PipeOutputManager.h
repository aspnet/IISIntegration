// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

class PipeOutputManager : public IOutputManager
{
public:
    PipeOutputManager();
    ~PipeOutputManager();

    // Inherited via ILoggerProvider
    virtual HRESULT Start() override;
    virtual void NotifyStartupComplete() override;

    // Inherited via IOutputManager
    virtual bool GetStdOutContent(STRU* struStdOutput) override;

    VOID ReadStdErrHandleInternal(VOID);

    static
    VOID ReadStdErrHandle(LPVOID pContext);

    VOID Dispose();
private:
    // The std log file handle
    HANDLE                          m_hLogFileHandle;
    HANDLE                          m_hErrReadPipe;
    HANDLE                          m_hErrWritePipe;
    STRU                            m_struLogFilePath;
    STRU                            m_struExeLocation;
    HANDLE                          m_hErrThread;
    CHAR                            m_pzFileContents[4096] = { 0 };
    DWORD                           m_dwStdErrReadTotal;
    BOOL                            m_fDisposed;
    int                             m_fdStdOut;
    int                             m_fdStdErr;
    FILE*                           m_fp;
};

