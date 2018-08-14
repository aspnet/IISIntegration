// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include <cstdio>

class StdWrapper
{
public:
    StdWrapper(FILE* outputStream, DWORD nHandle, HANDLE pipeHandle, HANDLE previousStdOut);
    ~StdWrapper();
    FILE* SetupRedirection();
    HRESULT StopRedirection() const;

    int previousFileDescriptor;
    FILE* stdStream;
    DWORD nHandle;
    HANDLE writerHandle;
    HANDLE previousStdOut;
    FILE* redirectedFile;
};

