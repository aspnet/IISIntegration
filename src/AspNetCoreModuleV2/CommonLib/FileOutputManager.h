// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

class FileOutputManager : public IOutputManager
{
public:
    FileOutputManager(PCWSTR pwzStdOutLogFileName, PCWSTR pwzApplciationpath);
    ~FileOutputManager();

    virtual bool GetStdOutContent(STRU* struStdOutput) override;
    virtual HRESULT Start() override;
    virtual void NotifyStartupComplete() override;

private:
    HANDLE m_hLogFileHandle;
    STTIMER m_Timer;
    STRU m_struStdOutLogFileName;
    STRU m_struLogFilePath;
    STRU m_struApplicationPath;
    int m_fdStdOut;
    int m_fdStdErr;
};

