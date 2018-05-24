// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

std::wstring
Helpers::CreateRandomValue()
{
    int randomValue = rand();
    return std::to_wstring(randomValue);
}

std::wstring
Helpers::CreateRandomTempDirectory()
{
    PWSTR tempPath = new WCHAR[256];
    GetTempPath(256, tempPath);
    std::wstring wstringPath(tempPath);

    return wstringPath.append(Helpers::CreateRandomValue()).append(L"\\");
}

void
Helpers::DeleteDirectory(std::wstring directory)
{
    std::experimental::filesystem::remove_all(directory);
}

std::wstring
Helpers::ReadFileContent(std::wstring file)
{
    std::wcout << file << std::endl;

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::fstream t(file);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return std::wstring(converter.from_bytes(buffer.str()));
}
