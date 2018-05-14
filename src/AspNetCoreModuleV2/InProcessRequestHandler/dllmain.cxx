// dllmain.cpp : Defines the entry point for the DLL application.
#include "precomp.hxx"
#include <IPHlpApi.h>
#include <VersionHelpers.h>

BOOL                g_fGlobalInitialize = FALSE;
BOOL                g_fProcessDetach = FALSE;
DWORD               g_dwAspNetCoreDebugFlags = 0;
DWORD               g_dwDebugFlags = 0;
DWORD               g_dwTlsIndex = TLS_OUT_OF_INDEXES;
SRWLOCK             g_srwLockRH;
HINTERNET           g_hWinhttpSession = NULL;
IHttpServer *       g_pHttpServer = NULL;
HINSTANCE           g_hWinHttpModule;
HINSTANCE           g_hAspNetCoreModule;
HANDLE              g_hEventLog = NULL;


VOID
InitializeGlobalConfiguration(
    IHttpServer * pServer
)
{
    HKEY hKey;
    BOOL fLocked = FALSE;

    if (!g_fGlobalInitialize)
    {
        AcquireSRWLockExclusive(&g_srwLockRH);
        fLocked = TRUE;

        if (g_fGlobalInitialize)
        {
            // Done by another thread
            goto Finished;
        }

        g_pHttpServer = pServer;
        if (pServer->IsCommandLineLaunch())
        {
            g_hEventLog = RegisterEventSource(NULL, ASPNETCORE_IISEXPRESS_EVENT_PROVIDER);
        }
        else
        {
            g_hEventLog = RegisterEventSource(NULL, ASPNETCORE_EVENT_PROVIDER);
        }

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\IIS Extensions\\IIS AspNetCore Module\\Parameters",
            0,
            KEY_READ,
            &hKey) == NO_ERROR)
        {
            DWORD dwType;
            DWORD dwData;
            DWORD cbData;

            cbData = sizeof(dwData);
            if ((RegQueryValueEx(hKey,
                L"DebugFlags",
                NULL,
                &dwType,
                (LPBYTE)&dwData,
                &cbData) == NO_ERROR) &&
                (dwType == REG_DWORD))
            {
                g_dwAspNetCoreDebugFlags = dwData;
            }
            RegCloseKey(hKey);
        }

        g_fGlobalInitialize = TRUE;
    }
Finished:
    if (fLocked)
    {
        ReleaseSRWLockExclusive(&g_srwLockRH);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        InitializeSRWLock(&g_srwLockRH);
        break;
    case DLL_PROCESS_DETACH:
        g_fProcessDetach = TRUE;
    default:
        break;
    }
    return TRUE;
}

HRESULT
__stdcall
CreateApplication(
    _In_  IHttpServer        *pServer,
    _In_  ASPNETCORE_CONFIG  *pConfig,
    _Out_ IAPPLICATION       **ppApplication
)
{
    HRESULT      hr = S_OK;
    IAPPLICATION *pApplication = NULL;

    // Initialze some global variables here
    InitializeGlobalConfiguration(pServer);

    if (pConfig->QueryHostingModel() == APP_HOSTING_MODEL::HOSTING_IN_PROCESS)
    {
        pApplication = new IN_PROCESS_APPLICATION(pServer, pConfig);
        if (pApplication == NULL)
        {
            hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
            goto Finished;
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        goto Finished;
    }

    *ppApplication = pApplication;

Finished:
    return hr;
}
