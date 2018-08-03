#pragma once

#include <cstdio>

class PipeWrapper
{
public:
    PipeWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle);
    ~PipeWrapper();
    HRESULT SetupRedirection();
    HRESULT StopRedirection();

    int previousFileDescriptor;
    FILE* stdStream;
    DWORD nHandle;
    HANDLE pipeHandle;
    FILE* redirectedFile;
};

