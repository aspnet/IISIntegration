// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

IOutputManager*
LoggingHelpers::CreateLoggingProvider(
    bool fIsLoggingEnabled,
    bool fIsConsoleWindow,
    PCWSTR pwzStdOutFileName,
    PCWSTR pwzApplicationPath
)
{
    if (fIsConsoleWindow)
    {
        return new NullConsoleManager;
    }
    else if (fIsLoggingEnabled)
    {
        return new FileOutputManager(pwzStdOutFileName, pwzApplicationPath);
    }
    else
    {
        return new PipeOutputManager;
    }
}
