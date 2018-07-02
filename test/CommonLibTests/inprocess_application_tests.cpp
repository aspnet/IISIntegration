// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

#include "stdafx.h"

#include <array>
#include "inprocessapplication.h"
#include "fakeclasses.h"

// Externals defined in inprocess
BOOL       g_fProcessDetach;
HANDLE     g_hEventLog;

namespace InprocessTests
{
    TEST(InProcessTest, NoNullRefForExePath)
    {
        auto server = new MockHttpServer();
        auto requestHandlerConfig = MockRequestHandlerConfig::CreateConfig();

        std::wstring exePath(L"hello");

        std::array<APPLICATION_PARAMETER, 1> parameters{
            {"InProcessExeLocation", exePath.data()}
        };

        auto config = std::shared_ptr<REQUESTHANDLER_CONFIG>(requestHandlerConfig);
        IN_PROCESS_APPLICATION *app = new IN_PROCESS_APPLICATION(server, config, parameters.data(), 1);

        ASSERT_STREQ(app->QueryExeLocation(), L"hello");
    }
}
