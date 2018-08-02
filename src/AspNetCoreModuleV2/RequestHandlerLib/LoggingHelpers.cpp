// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "LoggingHelpers.h"
#include "IOutputManager.h"
#include "FileOutputManager.h"
#include "PipeOutputManager.h"
#include "NullOutputManager.h"
#include <fcntl.h>
#include "debugutil.h"
#include "HandleWrapper.h"

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

FILE*
LoggingHelpers::ReReadStdFileNo(DWORD nHandle, FILE* originalFile)
{
    FILE* file = NULL;
    int fileDescriptor = -1;
    HANDLE stdHandle;
    DuplicateHandle(GetCurrentProcess(), GetStdHandle(nHandle), GetCurrentProcess(), &stdHandle, 0, TRUE, DUPLICATE_SAME_ACCESS);

    if (stdHandle != INVALID_HANDLE_VALUE)
    {
        fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
        if (fileDescriptor != -1)
        {
            file = _fdopen(fileDescriptor, "w");
            if (file != NULL)
            {
                int dup2Result = _dup2(_fileno(file), _fileno(originalFile));
                if (dup2Result == 0)
                {
                    setvbuf(originalFile, NULL, _IONBF, 0);
                }
            }
        }
    }
    return file;
}
