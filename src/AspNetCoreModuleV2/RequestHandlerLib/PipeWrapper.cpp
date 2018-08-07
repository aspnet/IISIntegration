// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "PipeWrapper.h"
#include "exceptions.h"
#include "LoggingHelpers.h"

PipeWrapper::PipeWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle, HANDLE previousStdOut)
    : stdStream(outputStream),
    nHandle(nHandle),
    writerHandle(pipeHandle),
    previousStdOut(previousStdOut)
{
}

PipeWrapper::~PipeWrapper() = default;

HRESULT
PipeWrapper::
SetupRedirection()
{
    // Duplicate the current stdstream to be used for restoring. 
    previousFileDescriptor = _dup(_fileno(stdStream));

    // in IIS, stdout and stderr are both invalid initially, so _dup will return -1
    // Open a null file to restore back and save the previous file descriptor
    if (previousFileDescriptor == -1)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stdStream);
        previousFileDescriptor = _fileno(stdStream);
    }

    // set the std handle to the writer
    //RETURN_LAST_ERROR_IF(!SetStdHandle(nHandle, writerHandle));


    // After setting the std handle, we need to set stdout/stderr to the current
    // output/error handle.
    redirectedFile = LoggingHelpers::ReReadStdFileNo(nHandle, stdStream, writerHandle);

    return S_OK;
}

HRESULT
PipeWrapper::StopRedirection() const
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
