// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include <cstdio>

class PipeWrapper
{
public:
    PipeWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle, HANDLE previousStdOut);
    ~PipeWrapper();
    HRESULT SetupRedirection();
    HRESULT StopRedirection() const;

    int previousFileDescriptor;
    FILE* stdStream;
    DWORD nHandle;
    HANDLE writerHandle;
    HANDLE previousStdOut;
    FILE* redirectedFile;
};

