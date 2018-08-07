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
LoggingHelpers::ReReadStdFileNo(DWORD nHandle, FILE* originalFile, HANDLE writerHandle)
{
    FILE* file = nullptr;
    HANDLE stdHandle;

    // As both stdout and stderr will point to the same handle,
    // we need to duplicate the handle for both of these. This is because
    // on dll unload, all currently open file handles will try to be closed
    // which will cause an exception as the stdout handle will be closed twice.

    DuplicateHandle(
        /* hSourceProcessHandle*/ GetCurrentProcess(),
        /* hSourceHandle */ writerHandle,
        /* hTargetProcessHandle */ GetCurrentProcess(),
        /* lpTargetHandle */&stdHandle,
        /* dwDesiredAccess */ 0, // dwDesired is ignored if DUPLICATE_SAME_ACCESS is specified
        /* bInheritHandle */ TRUE,
        /* dwOptions  */ DUPLICATE_SAME_ACCESS);


    // From the handle, we will get a file descriptor which will then be used
    // to get a FILE*. 
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
        const auto fileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(stdHandle), _O_TEXT);

        if (fileDescriptor != -1)
        {
            file = _fdopen(fileDescriptor, "w");

            if (file != nullptr)
            {
                // Set stdout/stderr to the newly created file output.
                // original file is stdout/stderr
                // file is a new file we opened
                auto fileNo = _fileno(file);
                auto fileNoStd = _fileno(originalFile);
                const auto dup2Result = _dup2(fileNo, fileNoStd);

                if (dup2Result == 0)
                {
                    // Removes buffering from the output
                    setvbuf(originalFile, nullptr, _IONBF, 0);
                }
            }
        }
    }

    SetStdHandle(nHandle, stdHandle);

    return file;
}

FILE*
LoggingHelpers::UndoStdFileHandle(DWORD nHandle, FILE* originalFile)
{
    FILE* file = nullptr;
    HANDLE stdHandle = GetStdHandle(nHandle);
    // As both stdout and stderr will point to the same handle,
    // we need to duplicate the handle for both of these. This is because
    // on dll unload, all currently open file handles will try to be closed
    // which will cause an exception as the stdout handle will be closed twice.
   

    // From the handle, we will get a file descriptor which will then be used
    // to get a FILE*. 
    if (stdHandle != INVALID_HANDLE_VALUE)
    {
        const auto fileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(stdHandle), _O_TEXT);
        if (fileDescriptor != -1)
        {
            file = _fdopen(fileDescriptor, "w");
            if (file != nullptr)
            {
                // Set stdout/stderr to the newly created file output.
                const auto dup2Result = _dup2(_fileno(file), _fileno(originalFile));
                if (dup2Result == 0)
                {
                    // Removes buffering from the output
                    setvbuf(originalFile, nullptr, _IONBF, 0);
                }
                else
                {
                    auto error = GetLastError();
                    if (error)
                    {
                        LOG_INFO("");
                    }
                }
            }
        }
    }

    return file;
}
