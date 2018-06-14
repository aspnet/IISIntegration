// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "inprocesshandler.h"
#include "inprocessapplication.h"
#include "aspnetcore_event.h"

ALLOC_CACHE_HANDLER * IN_PROCESS_HANDLER::sm_pAlloc = NULL;

IN_PROCESS_HANDLER::IN_PROCESS_HANDLER(
    _In_ IHttpContext   *pW3Context,
    _In_ IN_PROCESS_APPLICATION    *pApplication
):  m_pW3Context(pW3Context),
    m_pApplication(pApplication)
{
    m_fManagedRequestComplete = FALSE;
}

IN_PROCESS_HANDLER::~IN_PROCESS_HANDLER()
{
    //todo
}

__override
REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::OnExecuteRequestHandler()
{
    // First get the in process Application
    HRESULT hr;

    hr = m_pApplication->LoadManagedApplication();

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
        m_pW3Context->GetResponse()->SetStatus(500, "Internal Server Error", 0, hr);
        return REQUEST_NOTIFICATION_STATUS::RQ_NOTIFICATION_FINISH_REQUEST;
    }

    // FREB log

    if (ANCMEvents::ANCM_START_APPLICATION_SUCCESS::IsEnabled(m_pW3Context->GetTraceContext()))
    {
        ANCMEvents::ANCM_START_APPLICATION_SUCCESS::RaiseEvent(
            m_pW3Context->GetTraceContext(),
            NULL,
            L"InProcess Application");
    }

    //SetHttpSysDisconnectCallback();
    return m_pApplication->OnExecuteRequest(m_pW3Context, this);
}

// static
void * IN_PROCESS_HANDLER::operator new(size_t)
{
    DBG_ASSERT(sm_pAlloc != NULL);
    if (sm_pAlloc == NULL)
    {
        return NULL;
    }
    return sm_pAlloc->Alloc();
}

// static
void IN_PROCESS_HANDLER::operator delete(void * pMemory)
{
    DBG_ASSERT(sm_pAlloc != NULL);
    if (sm_pAlloc != NULL)
    {
        sm_pAlloc->Free(pMemory);
    }
}

// static
HRESULT
IN_PROCESS_HANDLER::StaticInitialize( VOID )
/*++

Routine Description:

Global initialization routine for IN_PROCESS_HANDLER

Return Value:

HRESULT

--*/
{
    HRESULT                         hr = S_OK;
    ALLOC_CACHE_HANDLER*            pAlloc = NULL;
    if (sm_pAlloc == NULL)
    {
        pAlloc = new ALLOC_CACHE_HANDLER;
        if (sm_pAlloc == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto Failure;
        }

        hr = pAlloc->Initialize(sizeof(IN_PROCESS_HANDLER),
            64); // nThreshold
        if (FAILED(hr))
        {
            goto Failure;
        }

        AcquireSRWLockExclusive(&g_srwLockRH);
        if (sm_pAlloc == NULL)
        {
            sm_pAlloc = pAlloc;
        }
        ReleaseSRWLockExclusive(&g_srwLockRH);

        pAlloc = NULL;
    }

Failure:

    if (pAlloc != NULL)
    {
        delete pAlloc;
        pAlloc = NULL;
    }

    return hr;
}
__override
REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::OnAsyncCompletion(
    DWORD       cbCompletion,
    HRESULT     hrCompletionStatus
)
{
    // OnAsyncCompletion must call into the application if there was a error. We will redo calls
    // to Read/Write if we called cancelIo on the IHttpContext.
    return m_pApplication->OnAsyncCompletion(cbCompletion, hrCompletionStatus, this);
}

VOID
IN_PROCESS_HANDLER::TerminateRequest(
    bool    fClientInitiated
)
{
    UNREFERENCED_PARAMETER(fClientInitiated);
    //todo
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

VOID
IN_PROCESS_HANDLER::IndicateManagedRequestComplete(
    VOID
)
{
    m_fManagedRequestComplete = TRUE;
}

REQUEST_NOTIFICATION_STATUS
IN_PROCESS_HANDLER::QueryAsyncCompletionStatus(
    VOID
)
{
    return m_requestNotificationStatus;
}

VOID
IN_PROCESS_HANDLER::SetAsyncCompletionStatus(
    REQUEST_NOTIFICATION_STATUS requestNotificationStatus
)
{
    m_requestNotificationStatus = requestNotificationStatus;
}

VOID
IN_PROCESS_HANDLER::SetManagedHttpContext(
    PVOID pManagedHttpContext
)
{
    m_pManagedHttpContext = pManagedHttpContext;
}
