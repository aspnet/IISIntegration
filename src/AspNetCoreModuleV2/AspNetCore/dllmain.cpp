// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "applicationinfo.h"
#include "applicationmanager.h"
#include "proxymodule.h"
#include "globalmodule.h"
#include "acache.h"
#include "utility.h"
#include "debugutil.h"
#include "resources.h"
#include "exceptions.h"
#include <corecrt_io.h>
#include <fcntl.h>

DECLARE_DEBUG_PRINT_OBJECT("aspnetcorev2.dll");

HANDLE              g_hEventLog = NULL;
BOOL                g_fRecycleProcessCalled = FALSE;
BOOL                g_fInShutdown = FALSE;

VOID
StaticCleanup()
{
    APPLICATION_MANAGER::Cleanup();
    if (g_hEventLog != NULL)
    {
        DeregisterEventSource(g_hEventLog);
        g_hEventLog = NULL;
    }

    DebugStop();
}

BOOL WINAPI DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        DebugInitialize(hModule);
        break;
    case DLL_PROCESS_DETACH:
        // IIS can cause dll detach to occur before we receive global notifications
        // For example, when we switch the bitness of the worker process,
        // this is a bug in IIS. To try to avoid AVs, we will set a global flag
        g_fInShutdown = TRUE;
        StaticCleanup();
    default:
        break;
    }

    return TRUE;
}
typedef INT(*hostfxr_get_native_search_directories_fn) (CONST INT argc, CONST PCWSTR* argv, PWSTR buffer, DWORD buffer_size, DWORD* required_buffer_size);
DWORD WINAPI MyThreadFunction(LPVOID lpParam);

HANDLE handle = INVALID_HANDLE_VALUE;


DWORD WINAPI
MyThreadFunction(
    LPVOID
)
{
    CHAR arr[300] = { 0 };
    DWORD dwBytesRead;
    DWORD m_dwStdErrReadTotal = 0;
    while (true)
    {
        // Fill a maximum of MAX_PIPE_READ_SIZE into a buffer.
        if (ReadFile(handle,
            &arr[m_dwStdErrReadTotal],
            300 - m_dwStdErrReadTotal,
            &dwBytesRead,
            nullptr))
        {
            m_dwStdErrReadTotal += dwBytesRead;
            if (m_dwStdErrReadTotal >= 300)
            {
                break;
            }
        }
        else
        {
            return 0;
        }
    }
    return 0;
}


HRESULT
__stdcall
RegisterModule(
DWORD                           dwServerVersion,
IHttpModuleRegistrationInfo *   pModuleInfo,
IHttpServer *                   pHttpServer
) try
/*++

Routine description:

Function called by IIS immediately after loading the module, used to let
IIS know what notifications the module is interested in

Arguments:

dwServerVersion - IIS version the module is being loaded on
pModuleInfo - info regarding this module
pHttpServer - callback functions which can be used by the module at
any point

Return value:

HRESULT

--*/
{
    HRESULT                             hr = S_OK;
    HKEY                                hKey;
    BOOL                                fDisableANCM = FALSE;
    ASPNET_CORE_PROXY_MODULE_FACTORY *  pFactory = NULL;
    ASPNET_CORE_GLOBAL_MODULE *         pGlobalModule = NULL;

    {

        while (!IsDebuggerPresent())
        {
            Sleep(100);
        }
        SECURITY_ATTRIBUTES     saAttr = { 0 };
        HANDLE                  hStdErrReadPipe;
        HANDLE                  hStdErrWritePipe;

        //AllocConsole();

        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        CreatePipe(&hStdErrReadPipe, &hStdErrWritePipe, &saAttr, 0 /*nSize*/);
        freopen_s((FILE**)stdout, "nul", "w", stdout);
        freopen_s((FILE**)stderr, "nul", "w", stderr);

        // STDOUT
        {
            _dup(_fileno(stdout));
            FILE* file = nullptr;
            HANDLE stdHandle;

            // As both stdout and stderr will point to the same handle,
            // we need to duplicate the handle for both of these. This is because
            // on dll unload, all currently open file handles will try to be closed
            // which will cause an exception as the stdout handle will be closed twice.

            DuplicateHandle(
                /* hSourceProcessHandle*/ GetCurrentProcess(),
                /* hSourceHandle */ hStdErrWritePipe,
                /* hTargetProcessHandle */ GetCurrentProcess(),
                /* lpTargetHandle */&stdHandle,
                /* dwDesiredAccess */ 0, // dwDesired is ignored if DUPLICATE_SAME_ACCESS is specified
                /* bInheritHandle */ TRUE,
                /* dwOptions  */ DUPLICATE_SAME_ACCESS);


            // From the handle, we will get a file descriptor which will then be used
            // to get a FILE*. 
            if (stdHandle != INVALID_HANDLE_VALUE)
            {
                const auto fileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(stdHandle), _O_TEXT);

                if (fileDescriptor != -1)
                {
                    file = _fdopen(fileDescriptor, "w");

                    if (file != nullptr)
                    {
                        // Set stdout/stderr to the newly created file output.
                        // original file is stdout/stderr
                        // file is a new file we opened
                        auto fileNo = _fileno(file);
                        auto fileNoStd = _fileno(stdout);
                        const auto dup2Result = _dup2(fileNo, fileNoStd);

                        if (dup2Result == 0)
                        {
                            // Removes buffering from the output
                            setvbuf(stdout, nullptr, _IONBF, 0);
                        }
                    }
                }
            }

            SetStdHandle(STD_OUTPUT_HANDLE, stdHandle);

        }
        // STDERR

        {
            _dup(_fileno(stderr));
            FILE* file = nullptr;
            HANDLE stdHandle;

            // As both stdout and stderr will point to the same handle,
            // we need to duplicate the handle for both of these. This is because
            // on dll unload, all currently open file handles will try to be closed
            // which will cause an exception as the stdout handle will be closed twice.

            DuplicateHandle(
                /* hSourceProcessHandle*/ GetCurrentProcess(),
                /* hSourceHandle */ hStdErrWritePipe,
                /* hTargetProcessHandle */ GetCurrentProcess(),
                /* lpTargetHandle */&stdHandle,
                /* dwDesiredAccess */ 0, // dwDesired is ignored if DUPLICATE_SAME_ACCESS is specified
                /* bInheritHandle */ TRUE,
                /* dwOptions  */ DUPLICATE_SAME_ACCESS);


            // From the handle, we will get a file descriptor which will then be used
            // to get a FILE*. 
            if (stdHandle != INVALID_HANDLE_VALUE)
            {
                const auto fileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(stdHandle), _O_TEXT);

                if (fileDescriptor != -1)
                {
                    file = _fdopen(fileDescriptor, "w");

                    if (file != nullptr)
                    {
                        // Set stdout/stderr to the newly created file output.
                        // original file is stdout/stderr
                        // file is a new file we opened
                        auto fileNo = _fileno(file);
                        auto fileNoStd = _fileno(stderr);
                        const auto dup2Result = _dup2(fileNo, fileNoStd);

                        if (dup2Result == 0)
                        {
                            // Removes buffering from the output
                            setvbuf(stderr, nullptr, _IONBF, 0);
                        }
                    }
                }
            }

            SetStdHandle(STD_ERROR_HANDLE, stdHandle);
        }
        handle = hStdErrReadPipe;
        // Read the stderr handle on a separate thread until we get 4096 bytes.
        CreateThread(
            nullptr,       // default security attributes
            0,          // default stack size
            MyThreadFunction,
            NULL,       // thread function arguments
            0,          // default creation flags
            nullptr);      // receive thread identifier


        auto hmHostFxrDll = LoadLibraryW(L"C:\\Users\\jukotali\\.dotnet\\x64\\host\\fxr\\2.2.0-preview1-26618-02\\hostfxr.dll"); // path to dll.
        const auto pFnHostFxrSearchDirectories = (hostfxr_get_native_search_directories_fn)
            GetProcAddress(hmHostFxrDll, "hostfxr_get_native_search_directories");
        unsigned long dwBufferSize = 10000;
        unsigned long dwRequiredBufferSize;

        pFnHostFxrSearchDirectories(
            0,
            NULL,
            NULL,
            dwBufferSize,
            &dwRequiredBufferSize
        );
        fprintf(stdout, "aspnetcore stdout");
        fprintf(stderr, "aspnetcore stderr");


        FlushFileBuffers(hStdErrWritePipe);
    }

    UNREFERENCED_PARAMETER(dwServerVersion);

    if (pHttpServer->IsCommandLineLaunch())
    {
        g_hEventLog = RegisterEventSource(NULL, ASPNETCORE_IISEXPRESS_EVENT_PROVIDER);
    }
    else
    {
        g_hEventLog = RegisterEventSource(NULL, ASPNETCORE_EVENT_PROVIDER);
    }

    // check whether the feature is disabled due to security reason
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\IIS Extensions\\IIS AspNetCore Module V2\\Parameters",
        0,
        KEY_READ,
        &hKey) == NO_ERROR)
    {
        DWORD dwType;
        DWORD dwData;
        DWORD cbData;

        cbData = sizeof(dwData);
        if ((RegQueryValueEx(hKey,
            L"DisableANCM",
            NULL,
            &dwType,
            (LPBYTE)&dwData,
            &cbData) == NO_ERROR) &&
            (dwType == REG_DWORD))
        {
            fDisableANCM = (dwData != 0);
        }

        RegCloseKey(hKey);
    }

    if (fDisableANCM)
    {
        // Logging
        STACK_STRU(strEventMsg, 256);
        if (SUCCEEDED(strEventMsg.SafeSnwprintf(
            ASPNETCORE_EVENT_MODULE_DISABLED_MSG)))
        {
            UTILITY::LogEvent(g_hEventLog,
                              EVENTLOG_WARNING_TYPE,
                              ASPNETCORE_EVENT_MODULE_DISABLED,
                              strEventMsg.QueryStr());
        }
        // this will return 500 error to client
        // as we did not register the module
        goto Finished;
    }

    //
    // Create the factory before any static initialization.
    // The ASPNET_CORE_PROXY_MODULE_FACTORY::Terminate method will clean any
    // static object initialized.
    //
    pFactory = new ASPNET_CORE_PROXY_MODULE_FACTORY;

    FINISHED_IF_FAILED(pModuleInfo->SetRequestNotifications(
                                  pFactory,
                                  RQ_EXECUTE_REQUEST_HANDLER,
                                  0));

    pFactory = NULL;

    FINISHED_IF_FAILED(APPLICATION_MANAGER::StaticInitialize(*pHttpServer));

    pGlobalModule = NULL;

    pGlobalModule = new ASPNET_CORE_GLOBAL_MODULE(APPLICATION_MANAGER::GetInstance());

    FINISHED_IF_FAILED(pModuleInfo->SetGlobalNotifications(
                                     pGlobalModule,
                                     GL_CONFIGURATION_CHANGE | // Configuration change trigers IIS application stop
                                     GL_STOP_LISTENING));   // worker process stop or recycle

    pGlobalModule = NULL;

    FINISHED_IF_FAILED(ALLOC_CACHE_HANDLER::StaticInitialize());

Finished:
    if (pGlobalModule != NULL)
    {
        delete pGlobalModule;
        pGlobalModule = NULL;
    }

    if (pFactory != NULL)
    {
        pFactory->Terminate();
        pFactory = NULL;
    }

    return hr;
}
CATCH_RETURN()
