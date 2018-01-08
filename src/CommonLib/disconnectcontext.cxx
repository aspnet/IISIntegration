// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#include "stdafx.h"

VOID
ASYNC_DISCONNECT_CONTEXT::NotifyDisconnect()
{
    REQUEST_HANDLER *pInitialValue = (REQUEST_HANDLER*)
        InterlockedExchangePointer((PVOID*)&m_pHandler, NULL);

    if (pInitialValue != NULL)
    {
        pInitialValue->TerminateRequest(TRUE);
        pInitialValue->DereferenceRequestHandler();
    }
}

VOID
ASYNC_DISCONNECT_CONTEXT::SetHandler(
    REQUEST_HANDLER *pHandler
)
{
    //
    // Take a reference on the forwarding handler.
    // This reference will be released on either of two conditions:
    //
    // 1. When the request processing ends, in which case a ResetHandler()
    // is called.
    // 
    // 2. When a disconnect notification arrives.
    //
    // We need to make sure that only one of them ends up dereferencing
    // the object.
    //

    DBG_ASSERT(pHandler != NULL);
    DBG_ASSERT(m_pHandler == NULL);

    pHandler->ReferenceRequestHandler();
    InterlockedExchangePointer((PVOID*)&m_pHandler, pHandler);
}

VOID
ASYNC_DISCONNECT_CONTEXT::ResetHandler(
    VOID
)
{
    REQUEST_HANDLER *pInitialValue = (REQUEST_HANDLER*)
        InterlockedExchangePointer((PVOID*)&m_pHandler, NULL);

    if (pInitialValue != NULL)
    {
        pInitialValue->DereferenceRequestHandler();
    }
}
