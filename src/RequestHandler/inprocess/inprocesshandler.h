// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

class IN_PROCESS_HANDLER : public REQUEST_HANDLER
{
public:
    IN_PROCESS_HANDLER(

        _In_ IHttpContext   *pW3Context,
        _In_ HTTP_MODULE_ID *pModuleId,
        _In_ APPLICATION    *pApplication);

    ~IN_PROCESS_HANDLER();

    __override
    REQUEST_NOTIFICATION_STATUS
    OnExecuteRequestHandler();

    __override
    REQUEST_NOTIFICATION_STATUS
    OnAsyncCompletion(
        DWORD       cbCompletion,
        HRESULT     hrCompletionStatus
    );

    __override
    VOID
    TerminateRequest(
        bool    fClientInitiated
    );

    HRESULT
    SetAsyncDisconnectContext(
        IHttpConnection *pClientConnection
    );

    PVOID
    QueryManagedHttpContext(
        VOID
    );

    VOID
    SetManagedHttpContext(
        PVOID pManagedHttpContext
    );

    IHttpContext*
    QueryHttpContext(
        VOID
    );

    BOOL
    QueryIsManagedRequestComplete(
        VOID
    );

    HRESULT
    IndicateManagedRequestComplete(
        _In_ REQUEST_NOTIFICATION_STATUS    requestNotificationStatus,
        _In_ DWORD                          cbBytes
    );

    REQUEST_NOTIFICATION_STATUS
    QueryAsyncCompletionStatus(
        VOID
    );

    VOID
    SetAsyncCompletionStatus(
        REQUEST_NOTIFICATION_STATUS requestNotificationStatus
    );

private:
    PVOID m_pManagedHttpContext;
    BOOL m_fManagedRequestComplete;
    REQUEST_NOTIFICATION_STATUS m_requestNotificationStatus;
};
