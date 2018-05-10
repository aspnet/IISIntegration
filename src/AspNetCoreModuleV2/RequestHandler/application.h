// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include "stdafx.h"

class APPLICATION : public IAPPLICATION
{

public:

    APPLICATION_STATUS
    QueryStatus() override
    {
        return m_status;
    }

    APPLICATION(
        _In_  ASPNETCORE_CONFIG*  pConfig,
        _In_  HINSTANCE hRequestHandlerModule) :
          m_cRefs(1),
          m_pConfig(pConfig),
          m_hRequestHandlerModule(hRequestHandlerModule)
    {
    }

    ~APPLICATION()
    {
        if (m_hRequestHandlerModule)
        {
            FreeLibrary(m_hRequestHandlerModule);
        }
    }

    VOID
    ReferenceApplication() override
    {
        InterlockedIncrement(&m_cRefs);
    }

    VOID
    DereferenceApplication() override
    {
        DBG_ASSERT(m_cRefs != 0);

        LONG cRefs = 0;
        if ((cRefs = InterlockedDecrement(&m_cRefs)) == 0)
        {
            delete this;
        }
    }

    ASPNETCORE_CONFIG*
    QueryConfig() const
    {
        return m_pConfig;
    }

protected:
    volatile APPLICATION_STATUS     m_status = APPLICATION_STATUS::UNKNOWN;
    ASPNETCORE_CONFIG*              m_pConfig;

private:
    mutable LONG                    m_cRefs;
    HINSTANCE                       m_hRequestHandlerModule;
};
