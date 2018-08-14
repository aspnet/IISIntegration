// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

#include "stdafx.h"
#include "gtest/internal/gtest-port.h"
#include "PipeOutputManager.h"

class FileManagerWrapper
{
public:
    PipeOutputManager * manager;
    FileManagerWrapper(PipeOutputManager* m)
        : manager(m)
    {
        manager->Start();
    }

    ~FileManagerWrapper()
    {
        delete manager;
    }
};

namespace PipeOutputManagerTests
{
    TEST(PipeManagerOutputTest, BasicFunctionalityCheck)
    {
        auto stdoutput = _fileno(stdout);
        auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        auto stderror = _fileno(stderr);
        auto stderrHandle = GetStdHandle(STD_ERROR_HANDLE);

        PCWSTR expected = L"test";
        STRA output;

        PipeOutputManager* pManager = new PipeOutputManager();

        ASSERT_EQ(S_OK, pManager->Start());
        fwprintf(stdout, expected);
        fwprintf(stderr, expected);

        ASSERT_EQ(S_OK, pManager->Stop());

        pManager->GetStdOutContent(&output);
        ASSERT_STREQ(output.QueryStr(), "testtest");
        delete pManager;

        auto stdoutput2 = _fileno(stdout);
        auto stdoutHandle2 = GetStdHandle(STD_OUTPUT_HANDLE);
        auto stderror2 = _fileno(stderr);
        auto stderrHandle2 = GetStdHandle(STD_ERROR_HANDLE);
        wprintf(expected, stdout);
    }

    TEST(PipeManagerOutputTest, BasicFunctionalityCheck2)
    {
        PCWSTR expected = L"test";
        STRA output;

        PipeOutputManager* pManager = new PipeOutputManager();

        ASSERT_EQ(S_OK, pManager->Start());
        fwprintf(stderr, expected);
        ASSERT_EQ(S_OK, pManager->Stop());

        pManager->GetStdOutContent(&output);
        ASSERT_STREQ(output.QueryStr(), "test");
        delete pManager;
    }

    TEST(PipeManagerOutputTest, CheckMaxPipeSize)
    {
        std::wstring test;
        STRA output;
        for (int i = 0; i < 3000; i++)
        {
            test.append(L"hello world");
        }

        PipeOutputManager* pManager = new PipeOutputManager();

        ASSERT_EQ(S_OK, pManager->Start());
        wprintf(test.c_str());
        ASSERT_EQ(S_OK, pManager->Stop());

        pManager->GetStdOutContent(&output);
        ASSERT_EQ(output.QueryCCH(), (DWORD)30000);
        delete pManager;
    }

    TEST(PipeManagerOutputTest, CheckMaxPipeSize2)
    {
        std::wstring test;
        STRA output;
        for (int i = 0; i < 3000; i++)
        {
            test.append(L"hello world");
        }

        PipeOutputManager* pManager = new PipeOutputManager();

        ASSERT_EQ(S_OK, pManager->Start());
        wprintf(test.c_str(), stderr);
        ASSERT_EQ(S_OK, pManager->Stop());

        pManager->GetStdOutContent(&output);
        ASSERT_EQ(output.QueryCCH(), (DWORD)30000);
        delete pManager;
    }

    TEST(PipeManagerOutputTest, SetInvalidHandlesForErrAndOut)
    {
        auto m_fdPreviousStdOut = _dup(_fileno(stdout));
        auto m_fdPreviousStdErr = _dup(_fileno(stderr));

        SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);
        SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);

        PCWSTR expected = L"test";

        PipeOutputManager* pManager = new PipeOutputManager();
        ASSERT_EQ(S_OK, pManager->Start());

        _dup2(m_fdPreviousStdOut, _fileno(stdout));
        _dup2(m_fdPreviousStdErr, _fileno(stderr));

        // Test will fail if we didn't redirect stdout back to a file descriptor.
        // This is because gtest relies on console output to know if a test succeeded or failed.
        // If the output still points to a file/pipe, the test (and all other tests after it) will fail.
        delete pManager;
    }

    TEST(PipeManagerOutputTest, CreateDeleteKeepOriginalStdOutAndStdErr)
    {
        for (int i = 0; i < 10; i++)
        {
            auto stdoutBefore = _fileno(stdout);
            auto stderrBefore = _fileno(stderr);
            auto stdoutHandleBefore = GetStdHandle(STD_OUTPUT_HANDLE);
            auto stderrHandleBefore = GetStdHandle(STD_ERROR_HANDLE);
            PCWSTR expected = L"test";
            STRA output;

            PipeOutputManager* pManager = new PipeOutputManager();

            ASSERT_EQ(S_OK, pManager->Start());
            fwprintf(stderr, expected);
            fwprintf(stdout, expected);

            ASSERT_EQ(S_OK, pManager->Stop());

            pManager->GetStdOutContent(&output);
            ASSERT_STREQ(output.QueryStr(), "testtest");
            ASSERT_EQ(stdoutBefore, _fileno(stdout));
            ASSERT_EQ(stderrBefore, _fileno(stderr));
            ASSERT_EQ(stdoutHandleBefore, GetStdHandle(STD_OUTPUT_HANDLE));
            ASSERT_EQ(stderrHandleBefore, GetStdHandle(STD_ERROR_HANDLE));
            delete pManager;
        }
        // When this returns, we get an AV from gtest.
    }

    TEST(PipeManagerOutputTest, CreateDeleteKeepOriginalStdErr)
    {
        for (int i = 0; i < 10; i++)
        {
            auto stdoutBefore = _fileno(stdout);
            auto stderrBefore = _fileno(stderr);
            auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            auto stderrHandle = GetStdHandle(STD_ERROR_HANDLE);
            PCWSTR expected = L"test";
            STRA output;

            PipeOutputManager* pManager = new PipeOutputManager();

            ASSERT_EQ(S_OK, pManager->Start());
            wprintf(expected);
            ASSERT_EQ(S_OK, pManager->Stop());

            pManager->GetStdOutContent(&output);
            ASSERT_STREQ(output.QueryStr(), "test");
            ASSERT_EQ(stdoutBefore, _fileno(stdout));

            ASSERT_EQ(stderrBefore, _fileno(stderr));

            delete pManager;
        }

        wprintf(L"Hello!");
    }
}

