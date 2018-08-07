// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include "IOutputManager.h"

class LoggingHelpers
{
public:

    static
    HRESULT
    CreateLoggingProvider(
        bool fLoggingEnabled,
        bool fEnablePipe,
        PCWSTR pwzStdOutFileName,
        PCWSTR pwzApplicationPath,
        _Out_ IOutputManager** outputManager
    );

    static
    FILE*
    ReReadStdFileNo(DWORD nHandle, FILE* originalFile, HANDLE writerHandle);
    static
        FILE* UndoStdFileHandle(const DWORD n_handle, FILE* const std_stream);
};

