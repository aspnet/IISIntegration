// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

NullConsoleManager::NullConsoleManager()
{
}


NullConsoleManager::~NullConsoleManager()
{

}

HRESULT NullConsoleManager::Start()
{
    // The process has console, e.g., IIS Express scenario
    return S_OK;
}

void NullConsoleManager::NotifyStartupComplete()
{
    // Do nothing.
}

bool NullConsoleManager::GetStdOutContent(STRU*)
{
    return false;
}

