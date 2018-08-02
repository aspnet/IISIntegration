
#include "stdafx.h"
#include "PipeWrapper.h"
#include "stdafx.h"
#include "exceptions.h"
#include <fcntl.h>
#include "LoggingHelpers.h"

PipeWrapper::PipeWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle)
    : outputStream(outputStream),
      nHandle(nHandle),
      pipeHandle(pipeHandle)
{
}

// Log if last error stuff in this class

PipeWrapper::~PipeWrapper()
{
}

HRESULT
PipeWrapper::
SetupRedirection()
{
    previousFileDescriptor = _dup(_fileno(outputStream));
    if (previousFileDescriptor == -1)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", outputStream);
        previousFileDescriptor = _fileno(outputStream);
    }

    RETURN_LAST_ERROR_IF(!SetStdHandle(nHandle, pipeHandle));

    redirectedFile = LoggingHelpers::ReReadStdFileNo(nHandle, outputStream);

    return S_OK;
}

HRESULT
PipeWrapper::
StopRedirection()
{
    fflush(outputStream);
    if (previousFileDescriptor >= 0)
    {
        LOG_LAST_ERROR_IF(!SetStdHandle(nHandle, (HANDLE)_get_osfhandle(previousFileDescriptor)));
    }

    fclose(redirectedFile);
    LoggingHelpers::ReReadStdFileNo(nHandle, outputStream);

    return S_OK;
}

