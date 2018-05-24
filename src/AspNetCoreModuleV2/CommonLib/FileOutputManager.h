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
    std::wstring m_wsStdOutLogFileName;
    std::wstring m_wsApplicationPath;
    STRU m_struLogFilePath;
    int m_fdStdOut;
    int m_fdStdErr;
};

