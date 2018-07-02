// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root for license information.

#pragma once

#include "stdafx.h"
#include "exceptions.h"
#include "requesthandler_config.h"
#include "filewatcher.h"
#include "..\CommonLib\aspnetcore_msg.h"
#include "..\CommonLib\resources.h"
#include "utility.h"

extern "C" FILE_WATCHER* g_pFileWatcher;
extern "C" HANDLE        g_hEventLog;

class APPLICATION : public IAPPLICATION
{

public:

    APPLICATION_STATUS
    QueryStatus() override
    {
        return m_status;
    }

    APPLICATION(std::shared_ptr<REQUESTHANDLER_CONFIG> pConfig)
        : m_cRefs(1),
        m_pFileWatcherEntry(NULL)
    {
        m_pConfig = pConfig;
    }

    virtual ~APPLICATION() override
    {

        if (m_pFileWatcherEntry != NULL)
        {
            m_pFileWatcherEntry->DereferenceFileWatcherEntry();
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

    VOID
    SetParameter(
        _In_ LPCWSTR           pzName,
        _In_ LPCWSTR           pzValue)
    override
    {
        UNREFERENCED_PARAMETER(pzName);
        UNREFERENCED_PARAMETER(pzValue);
    }

    HRESULT
    StartMonitoringAppOffline()
    {
        HRESULT hr = S_OK;
        if (g_pFileWatcher == NULL)
        {
            hr = E_INVALIDARG;
        }
        else
        {
            if (m_pFileWatcherEntry == NULL)
            {
                m_pFileWatcherEntry = new  FILE_WATCHER_ENTRY(g_pFileWatcher);
                hr = m_pFileWatcherEntry->Create(m_pConfig->QueryApplicationPhysicalPath()->QueryStr(), L"app_offline.htm", this, NULL);
            }
            else
            {
                // We never expect to resue the filewatcher entry or an application calls StartMonitoringAppOffline more than once
                hr = E_UNEXPECTED;
            }
        }

        if(FAILED(hr))
        {
            UTILITY::LogEventF(g_hEventLog,
                EVENTLOG_WARNING_TYPE,
                ASPNETCORE_EVENT_MONITOR_APPOFFLINE_ERROR,
                ASPNETCORE_EVENT_MONITOR_APPOFFLINE_ERROR_MSG,
                m_pConfig->QueryConfigPath()->QueryStr(),
                hr);
        }

        return hr;
    }

    VOID
    OnAppOffline() override
    {
        m_pFileWatcherEntry->StopMonitor();
        m_pFileWatcherEntry = NULL;
        m_status = APPLICATION_STATUS::OFFLINE;
        UTILITY::LogEventF(g_hEventLog,
            EVENTLOG_INFORMATION_TYPE,
            ASPNETCORE_EVENT_RECYCLE_APPOFFLINE,
            ASPNETCORE_EVENT_RECYCLE_APPOFFLINE_MSG,
            m_pConfig->QueryConfigPath()->QueryStr());
        Recycle();
    }

    REQUESTHANDLER_CONFIG*
    QueryConfig() const
    {
        return m_pConfig.get();
    }


protected:
    volatile APPLICATION_STATUS             m_status = APPLICATION_STATUS::UNKNOWN;
    FILE_WATCHER_ENTRY*                     m_pFileWatcherEntry;
    std::shared_ptr<REQUESTHANDLER_CONFIG>  m_pConfig;

private:
    mutable LONG                    m_cRefs;
};
