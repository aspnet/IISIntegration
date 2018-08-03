// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

#include "stdafx.h"
#include "gtest/internal/gtest-port.h"
#include "FileOutputManager.h"

extern PCWSTR g_moduleName = L"commonlibtest";

class FileManagerWrapper
{
public:
    FileOutputManager* manager;
    FileManagerWrapper(FileOutputManager* m)
        : manager(m)
    {
        manager->Start();
    }

    ~FileManagerWrapper()
    {
        manager->Stop();
        delete manager;
    }
};

namespace FileOutManagerStartupTests
{
    using ::testing::Test;
    class FileOutputManagerTest : public Test
    {
    protected:
        void
        Test(std::wstring fileNamePrefix, FILE* out)
        {
            PCWSTR expected = L"test";

            auto tempDirectory = TempDirectory();
            FileOutputManager* pManager = new FileOutputManager;
            pManager->Initialize(fileNamePrefix.c_str(), tempDirectory.path().c_str());
            {
                FileManagerWrapper wrapper(pManager);

                wprintf(expected, out);
            }

            for (auto & p : std::filesystem::directory_iterator(tempDirectory.path()))
            {
                std::wstring filename(p.path().filename());
                ASSERT_EQ(filename.substr(0, fileNamePrefix.size()), fileNamePrefix);

                //std::wstring content = Helpers::ReadFileContent(std::wstring(p.path()));
                //ASSERT_STREQ(content.c_str(), expected);
            }
        }
    };

    TEST_F(FileOutputManagerTest, WriteToFileCheckContentsWritten)
    {
        Test(L"", stdout);
        Test(L"log", stdout);
    }

    TEST_F(FileOutputManagerTest, StopWriteToFileCheckContentsWrittenErr)
    {
        Test(L"", stderr);
        Test(L"log", stderr);
    }
}

namespace FileOutManagerOutputTests
{
    TEST(Test, STDOUT)
    {
        auto stdoutput = _fileno(stdout);
        auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        auto stderror = _fileno(stderr);
        auto stderrHandle = GetStdHandle(STD_ERROR_HANDLE);

        PCWSTR expected = L"test";
        STRA output;

        auto tempDirectory = TempDirectory();

        FileOutputManager* pManager = new FileOutputManager();
        pManager->Initialize(L"", tempDirectory.path().c_str());

        ASSERT_EQ(S_OK, pManager->Start());
        wprintf(expected, stderr);
        ASSERT_EQ(S_OK, pManager->Stop());

        pManager->GetStdOutContent(&output);
        ASSERT_STREQ(output.QueryStr(), "test");
        delete pManager;

        auto stdoutput2 = _fileno(stdout);
        auto stdoutHandle2 = GetStdHandle(STD_OUTPUT_HANDLE);
        auto stderror2 = _fileno(stderr);
        auto stderrHandle2 = GetStdHandle(STD_ERROR_HANDLE);
    }
    /*TEST(FileOutManagerOutputTest, StdErr)
    {
        PCSTR expected = "test";

        auto tempDirectory = TempDirectory();

        FileOutputManager* pManager = new FileOutputManager;
        pManager->Initialize(L"", tempDirectory.path().c_str());
        {
            FileManagerWrapper wrapper(pManager);

            printf(expected, stderr);
            STRA straContent;
            ASSERT_TRUE(pManager->GetStdOutContent(&straContent));

            ASSERT_STREQ(straContent.QueryStr(), expected);
        }
    }

    TEST(FileOutManagerOutputTest, CheckFileOutput)
    {
        PCSTR expected = "test";

        auto tempDirectory = TempDirectory();

        FileOutputManager* pManager = new FileOutputManager;
        pManager->Initialize(L"", tempDirectory.path().c_str());
        {
            FileManagerWrapper wrapper(pManager);

            printf(expected);
            STRA straContent;
            ASSERT_TRUE(pManager->GetStdOutContent(&straContent));

            ASSERT_STREQ(straContent.QueryStr(), expected);
        }
    }

    TEST(FileOutManagerOutputTest, CapAt4KB)
    {
        PCSTR expected = "test";

        auto tempDirectory = TempDirectory();

        FileOutputManager* pManager = new FileOutputManager;
        pManager->Initialize(L"", tempDirectory.path().c_str());
        {
            FileManagerWrapper wrapper(pManager);

            for (int i = 0; i < 1200; i++)
            {
                printf(expected);
            }

            STRA straContent;
            ASSERT_TRUE(pManager->GetStdOutContent(&straContent));

            ASSERT_EQ(straContent.QueryCCH(), 4096);
        }
    }
*/
    TEST(FileOutManagerOutputTest, CreateDeleteKeepOriginalStdOutAndStdErr)
    {
        for (int i = 0; i < 10; i++)
        {
            auto tempDirectory = TempDirectory();

            auto stdoutBefore = _fileno(stdout);
            auto stderrBefore = _fileno(stderr);
            auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            auto stderrHandle = GetStdHandle(STD_ERROR_HANDLE);

            PCWSTR expected = L"test";
            STRA output;

            FileOutputManager* pManager = new FileOutputManager();
            pManager->Initialize(L"", tempDirectory.path().c_str());

            ASSERT_EQ(S_OK, pManager->Start());
            ASSERT_EQ(S_OK, pManager->Stop());

            ASSERT_EQ(stdoutBefore, _fileno(stdout));
            ASSERT_EQ(stderrBefore, _fileno(stderr));

            delete pManager;
        }
        // When this returns, we get an AV from gtest.
    }
}

