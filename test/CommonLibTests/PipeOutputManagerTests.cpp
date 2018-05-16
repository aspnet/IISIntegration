// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

#include "stdafx.h"
#include "gtest/internal/gtest-port.h"

class PipeManagerWrapper
{
public:
    PipeOutputManager * manager;
    PipeManagerWrapper(PipeOutputManager* m)
        : manager(m)
    {
        manager->Start();
    }

    ~PipeManagerWrapper()
    {
        delete manager;
    }
};

namespace PipeOutputManagerTests
{
    TEST(PipeManagerOutputTest, NotifyStartupCompleteCallsDispose)
    {
        PCWSTR expected = L"test";

        std::wstring tempDirectory = Helpers::CreateRandomTempDirectory();

        PipeOutputManager* pManager = new PipeOutputManager();
        ASSERT_EQ(S_OK, pManager->Start());

        pManager->NotifyStartupComplete();

        // This test will fail if we didn't redirect stdout back to a file descriptor.
    }
}

