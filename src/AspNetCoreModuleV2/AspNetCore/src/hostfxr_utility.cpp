// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "precomp.hxx"

//
// Runs a standalone appliction.
// The folder structure looks like this:
// Application/
//   hostfxr.dll
//   Application.exe
//   Application.dll
//   etc.
// We get the full path to hostfxr.dll and Application.dll and run hostfxr_main,
// passing in Application.dll.
// Assuming we don't need Application.exe as the dll is the actual application.
//
HRESULT
HOSTFXR_UTILITY::GetStandaloneHostfxrParameters(
    PCWSTR              pwzExeAbsolutePath, // includes .exe file extension.
    PCWSTR              pcwzApplicationPhysicalPath,
    PCWSTR              pcwzArguments,
    _Inout_ STRU*       struHostFxrDllLocation,
    _Out_ DWORD*        pdwArgCount,
    _Out_ PWSTR**       ppwzArgv
)
{
    HRESULT             hr = S_OK;
    STRU                struDllPath;
    STRU                struArguments;
    STRU                struHostFxrPath;
    STRU                struRuntimeConfigLocation;
    DWORD               dwPosition;

    // Obtain the app name from the processPath section.
    if ( FAILED( hr = struDllPath.Copy( pwzExeAbsolutePath ) ) )
    {
        goto Finished;
    }

    dwPosition = struDllPath.LastIndexOf( L'.', 0 );
    if ( dwPosition == -1 )
    {
        hr = E_FAIL;
        goto Finished;
    }

    hr = UTILITY::ConvertPathToFullPath( L".\\hostfxr.dll", pcwzApplicationPhysicalPath, &struHostFxrPath );
    if ( FAILED( hr ) )
    {
        goto Finished;
    }

	struDllPath.QueryStr()[dwPosition] = L'\0';
	if (FAILED(hr = struDllPath.SyncWithBuffer()))
	{
		goto Finished;
	}

    if ( !UTILITY::CheckIfFileExists( struHostFxrPath.QueryStr() ) )
    {
        // Most likely a full framework app.
        // Check that the runtime config file doesn't exist in the folder as another heuristic.
       /* if (FAILED(hr = struRuntimeConfigLocation.Copy(struDllPath)) ||
              FAILED(hr = struRuntimeConfigLocation.Append( L".runtimeconfig.json" )))
        {
            goto Finished;
        }
        if (!UTILITY::CheckIfFileExists(struRuntimeConfigLocation.QueryStr()))
        {*/

            hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
            goto Finished;

            //UTILITY::LogEventF(hEventLog,
            //                    EVENTLOG_ERROR_TYPE,
            //                    ASPNETCORE_EVENT_INPROCESS_FULL_FRAMEWORK_APP,
            //                    ASPNETCORE_EVENT_INPROCESS_FULL_FRAMEWORK_APP_MSG,
            //                    pcwzApplicationPhysicalPath,
            //                    hr);
        /*}*/
        //else
        //{
       // If a runtime config file does exist, report a file not found on the app.exe
       //hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
       //UTILITY::LogEventF(hEventLog,
       //                   EVENTLOG_ERROR_TYPE,
       //                   ASPNETCORE_EVENT_APPLICATION_EXE_NOT_FOUND,
       //                   ASPNETCORE_EVENT_APPLICATION_EXE_NOT_FOUND_MSG,
       //                   pcwzApplicationPhysicalPath,
       //                   hr);
       // //}

        //goto Finished;
    }

    if (FAILED(hr = struHostFxrDllLocation->Copy(struHostFxrPath)))
    {
        goto Finished;
    }


    if (FAILED(hr = struDllPath.Append(L".dll")))
    {
        goto Finished;
    }

    if (!UTILITY::CheckIfFileExists(struDllPath.QueryStr()))
    {
        // Treat access issue as File not found
        hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        goto Finished;
    }

    if (FAILED(hr = struArguments.Copy(struDllPath)) ||
        FAILED(hr = struArguments.Append(L" ")) ||
        FAILED(hr = struArguments.Append(pcwzArguments)))
    {
        goto Finished;
    }

    if (FAILED(hr = UTILITY::ParseHostfxrArguments(
        struArguments.QueryStr(),
        pwzExeAbsolutePath,
        pcwzApplicationPhysicalPath,
        pdwArgCount,
        ppwzArgv)))
    {
        // Adding Log
        goto Finished;
    }

Finished:

    return hr;
}

HRESULT
HOSTFXR_UTILITY::GetHostFxrParameters(
    HANDLE              hEventLog,
    PCWSTR              pcwzProcessPath,
    PCWSTR              pcwzApplicationPhysicalPath,
    PCWSTR              pcwzArguments,
    _Inout_ STRU*       struHostFxrDllLocation,
    _Out_ DWORD*        pdwArgCount,
    _Out_ BSTR**        pbstrArgv
)
{
    HRESULT                     hr = S_OK;
    STRU                        struSystemPathVariable;
    STRU                        struAbsolutePathToHostFxr;
    STRU                        struAbsolutePathToDotnet;
    STRU                        struEventMsg;
    STACK_STRU(struExpandedProcessPath, MAX_PATH);
    STACK_STRU(struExpandedArguments, MAX_PATH);

    // Copy and Expand the processPath and Arguments.
    if (FAILED(hr = struExpandedProcessPath.CopyAndExpandEnvironmentStrings(pcwzProcessPath))
        || FAILED(hr = struExpandedArguments.CopyAndExpandEnvironmentStrings(pcwzArguments)))
    {
        goto Finished;
    }

    // Convert the process path an absolute path to our current application directory.
    // If the path is already an absolute path, it will be unchanged.
    hr = UTILITY::ConvertPathToFullPath(
        struExpandedProcessPath.QueryStr(),
        pcwzApplicationPhysicalPath,
        &struAbsolutePathToDotnet
    );

    if (FAILED(hr))
    {
        goto Finished;
    }

    // Check if the absolute path is to dotnet or not.
    if (struAbsolutePathToDotnet.EndsWith(L"dotnet.exe") || struAbsolutePathToDotnet.EndsWith(L"dotnet"))
    {
        //
        // The processPath ends with dotnet.exe or dotnet
        // like: C:\Program Files\dotnet\dotnet.exe, C:\Program Files\dotnet\dotnet, dotnet.exe, or dotnet.
        // Get the absolute path to dotnet. If the path is already an absolute path, it will return that path
        //
        if (FAILED(hr = HOSTFXR_UTILITY::GetAbsolutePathToDotnet(&struAbsolutePathToDotnet))) // Make sure to append the dotnet.exe path correctly here (pass in regular path)?
        {
            goto Finished;
        }

        if (FAILED(hr = GetAbsolutePathToHostFxr(&struAbsolutePathToDotnet, hEventLog, &struAbsolutePathToHostFxr)))
        {
            goto Finished;
        }

        if (FAILED(hr = UTILITY::ParseHostfxrArguments(
            struExpandedArguments.QueryStr(),
            struAbsolutePathToDotnet.QueryStr(),
            pcwzApplicationPhysicalPath,
            pdwArgCount,
            pbstrArgv)))
        {
            // Adding Log
            goto Finished;
        }

        if (FAILED(hr = struHostFxrDllLocation->Copy(struAbsolutePathToHostFxr)))
        {
            goto Finished;
        }
    }
    else
    {
        //
        // The processPath is a path to the application executable
        // like: C:\test\MyApp.Exe or MyApp.Exe
        // Check if the file exists, and if it does, get the parameters for a standalone application
        //
        if (UTILITY::CheckIfFileExists(struAbsolutePathToDotnet.QueryStr()))
        {
            hr = GetStandaloneHostfxrParameters(
                struAbsolutePathToDotnet.QueryStr(),
                pcwzApplicationPhysicalPath,
                struExpandedArguments.QueryStr(),
                struHostFxrDllLocation,
                pdwArgCount,
                pbstrArgv);
        }
        else
        {
            //
            // If the processPath file does not exist and it doesn't include dotnet.exe or dotnet
            // then it is an invalid argument.
            //
            hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);;
            UTILITY::LogEventF(hEventLog,
                EVENTLOG_ERROR_TYPE,
                ASPNETCORE_EVENT_GENERAL_ERROR_MSG,
                ASPNETCORE_EVENT_INVALID_PROCESS_PATH_MSG,
                struExpandedProcessPath.QueryStr(),
                hr);
        }
    }

Finished:

    return hr;
}

HRESULT
HOSTFXR_UTILITY::GetAbsolutePathToDotnet(
    _Inout_ STRU* pStruAbsolutePathToDotnet
)
{
    HRESULT             hr = S_OK;

    //
    // If we are given an absolute path to dotnet.exe, we are done
    //
    if (UTILITY::CheckIfFileExists(pStruAbsolutePathToDotnet->QueryStr()))
    {
        goto Finished;
    }

    //
    // If the path was C:\Program Files\dotnet\dotnet
    // We need to try appending .exe and check if the file exists too.
    //
    if (FAILED(hr = pStruAbsolutePathToDotnet->Append(L".exe")))
    {
        goto Finished;
    }

    if (UTILITY::CheckIfFileExists(pStruAbsolutePathToDotnet->QueryStr()))
    {
        goto Finished;
    }

    // At this point, we are calling where.exe to find dotnet.
    // If we encounter any failures, try getting dotnet.exe from the
    // backup location.
    if (!InvokeWhereToFindDotnet(pStruAbsolutePathToDotnet))
    {
        hr = GetAbsolutePathToDotnetFromProgramFiles(pStruAbsolutePathToDotnet);
    }

Finished:

    return hr;
}

HRESULT
HOSTFXR_UTILITY::GetAbsolutePathToHostFxr(
    STRU* pStruAbsolutePathToDotnet,
    HANDLE hEventLog,
    STRU* pStruAbsolutePathToHostfxr
)
{
    HRESULT                     hr = S_OK;
    STRU                        struHostFxrPath;
    STRU                        struHostFxrSearchExpression;
    STRU                        struHighestDotnetVersion;
    STRU                        struEventMsg;
    std::vector<std::wstring>   vVersionFolders;
    DWORD                       dwPosition = 0;

    if (FAILED(hr = struHostFxrPath.Copy(pStruAbsolutePathToDotnet)))
    {
        goto Finished;
    }

    dwPosition = struHostFxrPath.LastIndexOf(L'\\', 0);
    if (dwPosition == -1)
    {
        hr = E_FAIL;
        goto Finished;
    }

    struHostFxrPath.QueryStr()[dwPosition] = L'\0';

    if (FAILED(hr = struHostFxrPath.SyncWithBuffer()) ||
        FAILED(hr = struHostFxrPath.Append(L"\\")))
    {
        goto Finished;
    }

    hr = struHostFxrPath.Append(L"host\\fxr");
    if (FAILED(hr))
    {
        goto Finished;
    }

    if (!UTILITY::DirectoryExists(&struHostFxrPath))
    {
        hr = ERROR_BAD_ENVIRONMENT;
        UTILITY::LogEventF(hEventLog,
            EVENTLOG_ERROR_TYPE,
            ASPNETCORE_EVENT_HOSTFXR_DIRECTORY_NOT_FOUND,
            struEventMsg.QueryStr(),
            ASPNETCORE_EVENT_HOSTFXR_DIRECTORY_NOT_FOUND_MSG,
            struHostFxrPath.QueryStr(),
            hr);
        goto Finished;
    }

    // Find all folders under host\\fxr\\ for version numbers.
    hr = struHostFxrSearchExpression.Copy(struHostFxrPath);
    if (FAILED(hr))
    {
        goto Finished;
    }

    hr = struHostFxrSearchExpression.Append(L"\\*");
    if (FAILED(hr))
    {
        goto Finished;
    }

    // As we use the logic from core-setup, we are opting to use std here.
    UTILITY::FindDotNetFolders(struHostFxrSearchExpression.QueryStr(), &vVersionFolders);

    if (vVersionFolders.size() == 0)
    {
        hr = HRESULT_FROM_WIN32(ERROR_BAD_ENVIRONMENT);
        UTILITY::LogEventF(hEventLog,
            EVENTLOG_ERROR_TYPE,
            ASPNETCORE_EVENT_HOSTFXR_DIRECTORY_NOT_FOUND,
            ASPNETCORE_EVENT_HOSTFXR_DIRECTORY_NOT_FOUND_MSG,
            struHostFxrPath.QueryStr(),
            hr);
        goto Finished;
    }

    hr = UTILITY::FindHighestDotNetVersion(vVersionFolders, &struHighestDotnetVersion);
    if (FAILED(hr))
    {
        goto Finished;
    }

    if (FAILED(hr = struHostFxrPath.Append(L"\\"))
        || FAILED(hr = struHostFxrPath.Append(struHighestDotnetVersion.QueryStr()))
        || FAILED(hr = struHostFxrPath.Append(L"\\hostfxr.dll")))
    {
        goto Finished;
    }

    if (!UTILITY::CheckIfFileExists(struHostFxrPath.QueryStr()))
    {
        // ASPNETCORE_EVENT_HOSTFXR_DLL_NOT_FOUND_MSG
        hr = HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
        UTILITY::LogEventF(hEventLog,
            EVENTLOG_ERROR_TYPE,
            ASPNETCORE_EVENT_HOSTFXR_DLL_NOT_FOUND,
            ASPNETCORE_EVENT_HOSTFXR_DLL_NOT_FOUND_MSG,
            struHostFxrPath.QueryStr(),
            hr);
        goto Finished;
    }

    if (FAILED(hr = pStruAbsolutePathToHostfxr->Copy(struHostFxrPath)))
    {
        goto Finished;
    }

Finished:
    return hr;
}

//
// Tries to call where.exe to find the location of dotnet.exe.
// Will check that the bitness of dotnet matches the current
// worker process bitness.
// Returns true if a valid dotnet was found, else false.
//
BOOL
HOSTFXR_UTILITY::InvokeWhereToFindDotnet(
    _Inout_ STRU* pStruAbsolutePathToDotnet
)
{
    HRESULT             hr = S_OK;
    // Arguments to call where.exe
    STARTUPINFOW        startupInfo = { 0 };
    PROCESS_INFORMATION processInformation = { 0 };
    SECURITY_ATTRIBUTES securityAttributes;

    CHAR                pzFileContents[READ_BUFFER_SIZE];
    HANDLE              hStdOutReadPipe = INVALID_HANDLE_VALUE;
    HANDLE              hStdOutWritePipe = INVALID_HANDLE_VALUE;
    LPWSTR              pwzDotnetName = NULL;
    DWORD               dwFilePointer;
    BOOL                fIsWow64Process;
    BOOL                fIsCurrentProcess64Bit;
    DWORD               dwExitCode;
    STRU                struDotnetSubstring;
    STRU                struDotnetLocationsString;
    DWORD               dwNumBytesRead;
    DWORD               dwBinaryType;
    INT                 index = 0;
    INT                 prevIndex = 0;
    BOOL                fProcessCreationResult = FALSE;
    BOOL                fResult = FALSE;

    // Set the security attributes for the read/write pipe
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.lpSecurityDescriptor = NULL;
    securityAttributes.bInheritHandle = TRUE;

    // Reset the path to dotnet as we will be using whether the string is
    // empty or not as state
    pStruAbsolutePathToDotnet->Reset();

    // Create a read/write pipe that will be used for reading the result of where.exe
    if (!CreatePipe(&hStdOutReadPipe, &hStdOutWritePipe, &securityAttributes, 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Finished;
    }
    if (!SetHandleInformation(hStdOutReadPipe, HANDLE_FLAG_INHERIT, 0))
    {
        hr = ERROR_FILE_INVALID;
        goto Finished;
    }

    // Set the stdout and err pipe to the write pipes.
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = hStdOutWritePipe;
    startupInfo.hStdError = hStdOutWritePipe;

    // CreateProcess requires a mutable string to be passed to commandline
    // See https://blogs.msdn.microsoft.com/oldnewthing/20090601-00/?p=18083/
    pwzDotnetName = SysAllocString(L"\"where.exe\" dotnet.exe");
    if (pwzDotnetName == NULL)
    {
        goto Finished;
    }

    // Create a process to invoke where.exe
    fProcessCreationResult = CreateProcessW(NULL,
        pwzDotnetName,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startupInfo,
        &processInformation
    );

    if (!fProcessCreationResult)
    {
        goto Finished;
    }

    // Wait for where.exe to return, waiting 2 seconds.
    if (WaitForSingleObject(processInformation.hProcess, 2000) != WAIT_OBJECT_0)
    {
        // Timeout occured, terminate the where.exe process and return.
        TerminateProcess(processInformation.hProcess, 2);
        hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        goto Finished;
    }

    //
    // where.exe will return 0 on success, 1 if the file is not found
    // and 2 if there was an error. Check if the exit code is 1 and set
    // a new hr result saying it couldn't find dotnet.exe
    //
    if (!GetExitCodeProcess(processInformation.hProcess, &dwExitCode))
    {
        goto Finished;
    }

    //
    // In this block, if anything fails, we will goto our fallback of
    // looking in C:/Program Files/
    //
    if (dwExitCode != 0)
    {
        goto Finished;
    }

    // Where succeeded.
    // Reset file pointer to the beginning of the file.
    dwFilePointer = SetFilePointer(hStdOutReadPipe, 0, NULL, FILE_BEGIN);
    if (dwFilePointer == INVALID_SET_FILE_POINTER)
    {
        goto Finished;
    }

    //
    // As the call to where.exe succeeded (dotnet.exe was found), ReadFile should not hang.
    // TODO consider putting ReadFile in a separate thread with a timeout to guarantee it doesn't block.
    //
    if (!ReadFile(hStdOutReadPipe, pzFileContents, READ_BUFFER_SIZE, &dwNumBytesRead, NULL))
    {
        goto Finished;
    }

    if (dwNumBytesRead >= READ_BUFFER_SIZE)
    {
        // This shouldn't ever be this large. We could continue to call ReadFile in a loop,
        // however if someone had this many dotnet.exes on their machine.
        goto Finished;
    }

    hr = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(hr = struDotnetLocationsString.CopyA(pzFileContents, dwNumBytesRead)))
    {
        goto Finished;
    }

    // Check the bitness of the currently running process
    // matches the dotnet.exe found.
    if (!IsWow64Process(GetCurrentProcess(), &fIsWow64Process))
    {
        // Calling IsWow64Process failed
        goto Finished;
    }
    if (fIsWow64Process)
    {
        // 32 bit mode
        fIsCurrentProcess64Bit = FALSE;
    }
    else
    {
        // Check the SystemInfo to see if we are currently 32 or 64 bit.
        SYSTEM_INFO systemInfo;
        GetNativeSystemInfo(&systemInfo);
        fIsCurrentProcess64Bit = systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
    }

    while (TRUE)
    {
        index = struDotnetLocationsString.IndexOf(L"\r\n", prevIndex);
        if (index == -1)
        {
            break;
        }
        if (FAILED(hr = struDotnetSubstring.Copy(&struDotnetLocationsString.QueryStr()[prevIndex], index - prevIndex)))
        {
            goto Finished;
        }
        // \r\n is two wchars, so add 2 here.
        prevIndex = index + 2;

        if (GetBinaryTypeW(struDotnetSubstring.QueryStr(), &dwBinaryType) &&
            fIsCurrentProcess64Bit == (dwBinaryType == SCS_64BIT_BINARY))
        {
            // The bitness of dotnet matched with the current worker process bitness.
            if (FAILED(hr = pStruAbsolutePathToDotnet->Copy(struDotnetSubstring)))
            {
                goto Finished;
            }
            fResult = TRUE;
            break;
        }
    }

Finished:

    if (hStdOutReadPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hStdOutReadPipe);
    }
    if (hStdOutWritePipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hStdOutWritePipe);
    }
    if (processInformation.hProcess != INVALID_HANDLE_VALUE)
    {
        CloseHandle(processInformation.hProcess);
    }
    if (processInformation.hThread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(processInformation.hThread);
    }
    if (pwzDotnetName != NULL)
    {
        SysFreeString(pwzDotnetName);
    }

    return fResult;
}


HRESULT
HOSTFXR_UTILITY::GetAbsolutePathToDotnetFromProgramFiles(
    _Inout_ STRU* pStruAbsolutePathToDotnet
)
{
    HRESULT hr = S_OK;
    BOOL fFound = FALSE;
    DWORD dwNumBytesRead = 0;
    DWORD dwPathSize = MAX_PATH;
    STRU struDotnetSubstring;

    while (!fFound)
    {
        if (FAILED(hr = struDotnetSubstring.Resize(dwPathSize)))
        {
            goto Finished;
        }

        dwNumBytesRead = GetEnvironmentVariable(L"ProgramFiles", struDotnetSubstring.QueryStr(), dwPathSize);
        if (dwNumBytesRead == 0)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto Finished;
        }
        else if (dwNumBytesRead >= dwPathSize)
        {
            //
            // The path to ProgramFiles should never be this long, but resize and try again.
            dwPathSize *= 2 + 30; // for dotnet substring
        }
        else
        {
            if (FAILED(hr = struDotnetSubstring.SyncWithBuffer()) ||
                FAILED(hr = struDotnetSubstring.Append(L"\\dotnet\\dotnet.exe")))
            {
                goto Finished;
            }
            if (!UTILITY::CheckIfFileExists(struDotnetSubstring.QueryStr()))
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                goto Finished;
            }
            if (FAILED(hr = pStruAbsolutePathToDotnet->Copy(struDotnetSubstring)))
            {
                goto Finished;
            }
            fFound = TRUE;
        }
    }

Finished:
    return hr;
}
