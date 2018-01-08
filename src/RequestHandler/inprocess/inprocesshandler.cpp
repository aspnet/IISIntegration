// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "..\precomp.hxx"

IN_PROCESS_HANDLER::IN_PROCESS_HANDLER(
    _In_ IHttpContext   *pW3Context,
    _In_ HTTP_MODULE_ID *pModuleId,
    _In_ APPLICATION    *pApplication
): REQUEST_HANDLER(pW3Context, pModuleId, pApplication)
{
    m_fManagedRequestComplete = FALSE;
}

IN_PROCESS_HANDLER::~IN_PROCESS_HANDLER()
{
}

__override
REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::OnExecuteRequestHandler()
{
    // First get the in process Application
    HRESULT                     hr = S_OK;
    IHttpConnection            *pClientConnection = NULL;
    BOOL                        fHandleSet = FALSE;
    BOOL                        fRequestLocked = FALSE;
    REQUEST_NOTIFICATION_STATUS retVal = RQ_NOTIFICATION_CONTINUE;

    // check connection
    pClientConnection = m_pW3Context->GetConnection();
    if (pClientConnection == NULL ||
        !pClientConnection->IsConnected())
    {
        hr = HRESULT_FROM_WIN32(WSAECONNRESET);
        goto Failure;
    }

    // Acquire lock before client disconnect callback may happen,
    // allowing OnExecuteRequestHandler to complete
    AcquireSRWLockShared(&m_RequestLock);
    fRequestLocked = TRUE;

    // The disconnect handler will hold onto the inprocesshandler reference. On post completion, we will remove
    // 
    if (FAILED(hr = SetAsyncDisconnectContext(pClientConnection)))
    {
        goto Failure;
    }

    fHandleSet = TRUE;

    hr = ((IN_PROCESS_APPLICATION*)m_pApplication)->LoadManagedApplication();
    if (FAILED(hr))
    {
        // TODO remove com_error?
        /*_com_error err(hr);
        if (ANCMEvents::ANCM_START_APPLICATION_FAIL::IsEnabled(m_pW3Context->GetTraceContext()))
        {
            ANCMEvents::ANCM_START_APPLICATION_FAIL::RaiseEvent(
                m_pW3Context->GetTraceContext(),
                NULL,
                err.ErrorMessage());
        }
        */
        //fInternalError = TRUE;
        goto Failure;
    }

    // FREB log
    
    if (ANCMEvents::ANCM_START_APPLICATION_SUCCESS::IsEnabled(m_pW3Context->GetTraceContext()))
    {
        ANCMEvents::ANCM_START_APPLICATION_SUCCESS::RaiseEvent(
            m_pW3Context->GetTraceContext(),
            NULL,
            L"InProcess Application");
    }

    retVal = ((IN_PROCESS_APPLICATION*)m_pApplication)->OnExecuteRequest(m_pW3Context, this);
    goto Finished;

Failure:
    if (m_pDisconnect != NULL)
    {
        if (fHandleSet)
        {
            m_pDisconnect->ResetHandler();
        }
        m_pDisconnect->CleanupStoredContext();
        m_pDisconnect = NULL;
    }
    retVal = RQ_NOTIFICATION_FINISH_REQUEST;
    // TODO make status codes and messages better.
    m_pW3Context->GetResponse()->SetStatus(500, "Internal Server Error", 0, hr);

Finished:
    if (fRequestLocked)
    {
        DBG_ASSERT(TlsGetValue(g_dwTlsIndex) == this);
        TlsSetValue(g_dwTlsIndex, NULL);
        ReleaseSRWLockShared(&m_RequestLock);
        DBG_ASSERT(TlsGetValue(g_dwTlsIndex) == NULL);
    }

    return retVal;
}

__override
REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::OnAsyncCompletion(
    DWORD       cbCompletion,
    HRESULT     hrCompletionStatus
)
{
    IN_PROCESS_APPLICATION* application = (IN_PROCESS_APPLICATION*)m_pApplication;
    if (application == NULL)
    {
        return RQ_NOTIFICATION_FINISH_REQUEST;
    }

    // OnAsyncCompletion must call into the application if there was a error. We will redo calls
    // to Read/Write if we called cancelIo on the IHttpContext.
    return application->OnAsyncCompletion(cbCompletion, hrCompletionStatus, this);
}

VOID
IN_PROCESS_HANDLER::TerminateRequest(
    bool    fClientInitiated
)
{
    // Guarantee we still hold onto the request handler when calling into managed.
    ReferenceRequestHandler();

    UNREFERENCED_PARAMETER(fClientInitiated);
    AcquireSRWLockExclusive(&m_RequestLock);
    // Set tls as close winhttp handle will immediately trigger
    // a winhttp callback on the same thread and we donot want to
    // acquire the lock again
    TlsSetValue(g_dwTlsIndex, this);
    DBG_ASSERT(TlsGetValue(g_dwTlsIndex) == this);

    // Call into managed to cancel the request
    IN_PROCESS_APPLICATION* application = (IN_PROCESS_APPLICATION*)m_pApplication;
    application->TerminateRequest(this);

    TlsSetValue(g_dwTlsIndex, NULL);
    ReleaseSRWLockExclusive(&m_RequestLock);
    DBG_ASSERT(TlsGetValue(g_dwTlsIndex) == NULL);

    DereferenceRequestHandler();
}

PVOID
IN_PROCESS_HANDLER::QueryManagedHttpContext(
    VOID
)
{
    return m_pManagedHttpContext;
}

BOOL
IN_PROCESS_HANDLER::QueryIsManagedRequestComplete(
    VOID
)
{
    return m_fManagedRequestComplete;
}

IHttpContext*
IN_PROCESS_HANDLER::QueryHttpContext(
    VOID
)
{
    return m_pW3Context;
}

HRESULT
IN_PROCESS_HANDLER::IndicateManagedRequestComplete(
    _In_ REQUEST_NOTIFICATION_STATUS    requestNotificationStatus,
    _In_ DWORD                          cbBytes
)
{
    HRESULT hr = S_OK;
    // TODO TlsGet and Set Value?
    AcquireSRWLockExclusive(&m_RequestLock);
    if (m_fManagedRequestComplete)
    {
        goto Finished;
    }
    // Stop the disconnect handler from firing, as the request has been completed by managed. 
    m_pDisconnect->ResetHandler();
    m_fManagedRequestComplete = TRUE;
    m_requestNotificationStatus = requestNotificationStatus;
    if (FAILED(hr = m_pW3Context->PostCompletion(cbBytes)))
    {
        goto Finished;
    }

Finished:
    ReleaseSRWLockExclusive(&m_RequestLock);
    return hr;
}

REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::QueryAsyncCompletionStatus(
    VOID
)
{
    return m_requestNotificationStatus;
}

VOID
IN_PROCESS_HANDLER::SetManangedHttpContext(
    PVOID pManagedHttpContext
)
{
    m_pManagedHttpContext = pManagedHttpContext;
}

HRESULT
IN_PROCESS_HANDLER::SetAsyncDisconnectContext
(
    IHttpConnection *pClientConnection
)
{
    HRESULT hr = S_OK;

    // Set client disconnect callback contract with IIS
    m_pDisconnect = static_cast<ASYNC_DISCONNECT_CONTEXT *>(
        pClientConnection->GetModuleContextContainer()->
        GetConnectionModuleContext(m_pModuleId));
    if (m_pDisconnect == NULL)
    {
        m_pDisconnect = new ASYNC_DISCONNECT_CONTEXT();
        if (m_pDisconnect == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto Finished;
        }

        hr = pClientConnection->GetModuleContextContainer()->
            SetConnectionModuleContext(m_pDisconnect,
                m_pModuleId);
        DBG_ASSERT(hr != HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED));
        if (FAILED(hr))
        {
            goto Finished;
        }
    }

    m_pDisconnect->SetHandler(this);

Finished:
    return hr;
}
