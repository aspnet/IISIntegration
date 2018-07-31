// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "LoggingHelpers.h"
#include "IOutputManager.h"
#include "FileOutputManager.h"
#include "PipeOutputManager.h"
#include "NullOutputManager.h"
#include <fcntl.h>

HRESULT
LoggingHelpers::CreateLoggingProvider(
    bool fIsLoggingEnabled,
    bool fEnablePipe,
    PCWSTR pwzStdOutFileName,
    PCWSTR pwzApplicationPath,
    _Out_ IOutputManager** outputManager
)
{
    HRESULT hr = S_OK;

    DBG_ASSERT(outputManager != NULL);

    try
    {
        if (fIsLoggingEnabled)
        {
            FileOutputManager* manager = new FileOutputManager;
            hr = manager->Initialize(pwzStdOutFileName, pwzApplicationPath);
            *outputManager = manager;
        }
        else if (fEnablePipe)
        {
            *outputManager = new PipeOutputManager;
        }
        else
        {
            *outputManager = new NullOutputManager;
        }
    }
    catch (std::bad_alloc&)
    {
        hr = E_OUTOFMEMORY;
    }

    return hr;
}

VOID
LoggingHelpers::ReReadStdOutFileNo()
{
    HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
        int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
        if (fileDescriptor != -1)
        {
            FILE* file = _fdopen(fileDescriptor, "w");
            if (file != NULL)
            {
                // This returns -1.
                int dup2Result = _dup2(_fileno(file), _fileno(stdout));
                if (dup2Result == 0)
                {
                    setvbuf(stdout, NULL, _IONBF, 0);
                }
            }
        }
    }
}

VOID
LoggingHelpers::ReReadStdErrFileNo()
{
    HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
        int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
        if (fileDescriptor != -1)
        {
            FILE* file = _fdopen(fileDescriptor, "w");
            if (file != NULL)
            {
                int dup2Result = _dup2(_fileno(file), _fileno(stderr));
                if (dup2Result == 0)
                {
                    setvbuf(stderr, NULL, _IONBF, 0);
                }
            }
        }
    }
}
