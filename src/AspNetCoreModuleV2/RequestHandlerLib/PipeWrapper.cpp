// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"
#include "PipeWrapper.h"
#include "exceptions.h"
#include "LoggingHelpers.h"

PipeWrapper::PipeWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle)
    : stdStream(outputStream),
      nHandle(nHandle),
      writerHandle(pipeHandle)
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

    // set the std handle to the  
    RETURN_LAST_ERROR_IF(!SetStdHandle(nHandle, writerHandle));

    // After setting the std handle, we need to set stdout/stderr to the current
    // output/error handle.
    redirectedFile = LoggingHelpers::ReReadStdFileNo(nHandle, stdStream);

    return S_OK;
}

HRESULT
PipeWrapper::StopRedirection() const
{
    // Restore the original std handle
    LOG_LAST_ERROR_IF(!SetStdHandle(nHandle, reinterpret_cast<HANDLE>(_get_osfhandle(previousFileDescriptor))));

    // Close the redirected file. 
    LOG_LAST_ERROR_IF(fclose(redirectedFile));

    // After setting the std handle, we need to set stdout/stderr to the current
    // output/error handle.
    LoggingHelpers::ReReadStdFileNo(nHandle, stdStream);

    return S_OK;
}
