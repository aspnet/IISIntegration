// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

class NullConsoleManager : public IOutputManager
{
public:
    NullConsoleManager();
    ~NullConsoleManager();

    virtual HRESULT Start() override;
    virtual void NotifyStartupComplete() override;
    virtual bool GetStdOutContent(STRU* struStdOutput) override;
};

