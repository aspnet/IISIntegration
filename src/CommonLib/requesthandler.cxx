// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

REQUEST_HANDLER::REQUEST_HANDLER(
    _In_ IHttpContext *pW3Context,
    _In_ HTTP_MODULE_ID  *pModuleId,
    _In_ APPLICATION  *pApplication)
    : m_cRefs(1)
{
    m_pW3Context = pW3Context;
    m_pApplication = pApplication;
    m_pModuleId = *pModuleId;
    InitializeSRWLock(&m_RequestLock);
}


REQUEST_HANDLER::~REQUEST_HANDLER()
{
    //
    // RemoveRequest() should already have been called and m_pDisconnect
    // has been freed or m_pDisconnect was never initialized.
    //
    // Disconnect notification cleanup would happen first, before
    // the FORWARDING_HANDLER instance got removed from m_pSharedhandler list.
    // The m_pServer cleanup would happen afterwards, since there may be a 
    // call pending from SHARED_HANDLER to  FORWARDING_HANDLER::SetStatusAndHeaders()
    // 
    DBG_ASSERT(m_pDisconnect == NULL);

    if (m_pDisconnect != NULL)
    {
        m_pDisconnect->ResetHandler();
        m_pDisconnect = NULL;
    }

    ReleaseSRWLockExclusive(&m_RequestLock);
}


VOID
REQUEST_HANDLER::ReferenceRequestHandler(
    VOID
) const
{
    InterlockedIncrement(&m_cRefs);
}


VOID
REQUEST_HANDLER::DereferenceRequestHandler(
    VOID
) const
{
    DBG_ASSERT(m_cRefs != 0);

    LONG cRefs = 0;
    if ((cRefs = InterlockedDecrement(&m_cRefs)) == 0)
    {
        delete this;
    }
}
