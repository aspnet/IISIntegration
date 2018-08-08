// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "StdWrapper.h"
#include "exceptions.h"
#include "LoggingHelpers.h"
#include <corecrt_io.h>

StdWrapper::StdWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle, HANDLE previousStdOut)
    : stdStream(outputStream),
    nHandle(nHandle),
    writerHandle(pipeHandle),
    previousStdOut(previousStdOut)
{
}

StdWrapper::~StdWrapper() = default;

HRESULT
StdWrapper::
SetupRedirection()
{
    // Duplicate the current stdstream to be used for restoring. 

    // in IIS, stdout and stderr are both invalid initially, so _dup will return -1
    // Open a null file to restore back and save the previous file descriptor
    if (_fileno(stdStream) == -2)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stdStream);
        // Why does this need to be duped
        previousFileDescriptor = _dup(_fileno(stdStream));
    }
    else
    {
        previousFileDescriptor = _dup(_fileno(stdStream));
    }

    // After setting the std handle, we need to set stdout/stderr to the current
    // output/error handle.
    redirectedFile = LoggingHelpers::ReReadStdFileNo(nHandle, stdStream, writerHandle);

    return S_OK;
}

HRESULT
StdWrapper::StopRedirection() const
{
    FILE* file = nullptr;

    //// Restore the original std handle
    //auto handleToClose = reinterpret_cast<HANDLE>(_get_osfhandle(3));
  
    // After setting the std handle, we need to set stdout/stderr to the current
    // output/error handle.

    file = _fdopen(previousFileDescriptor, "w");
    if (file != nullptr)
    {
        // Set stdout/stderr to the newly created file output.
        const auto dup2Result = _dup2(_fileno(file), _fileno(stdStream));
        if (dup2Result == 0)
        {
            // Removes buffering from the output
            setvbuf(stdStream, nullptr, _IONBF, 0);
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

    LOG_LAST_ERROR_IF(!SetStdHandle(nHandle, previousStdOut));

    // Close the redirected file. 
    LOG_LAST_ERROR_IF(fclose(redirectedFile));

    return S_OK;
}
