// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include "sttimer.h"
#include "IOutputManager.h"
#include "HandleWrapper.h"
#include "StdWrapper.h"
#include "stringa.h"
#include "stringu.h"

class FileOutputManager : public IOutputManager
{
    #define FILE_FLUSH_TIMEOUT 3000
    #define MAX_FILE_READ_SIZE 30000
public:
    FileOutputManager();
    ~FileOutputManager();

    HRESULT
    Initialize(PCWSTR pwzStdOutLogFileName, PCWSTR pwzApplciationpath);

    virtual bool GetStdOutContent(STRA* struStdOutput) override;
    virtual HRESULT Start() override;
    virtual HRESULT Stop() override;

private:
    HANDLE m_hLogFileHandle;
    STTIMER m_Timer;
    STRU m_wsStdOutLogFileName;
    STRU m_wsApplicationPath;
    STRU m_struLogFilePath;
    int m_fdPreviousStdOut;
    int m_fdPreviousStdErr;
    BOOL m_disposed;
    SRWLOCK m_srwLock;
    FILE* m_pStdout;
    FILE* m_pStderr;
    std::unique_ptr<StdWrapper>    stdoutWrapper;
    std::unique_ptr<StdWrapper>    stderrWrapper;
};
