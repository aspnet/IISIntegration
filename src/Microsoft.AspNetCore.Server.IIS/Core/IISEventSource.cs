// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System.Diagnostics.Tracing;
using System.Runtime.CompilerServices;

namespace Microsoft.AspNetCore.Server.IIS.Core
{
    [EventSource(Name = "Microsoft-AspNetCore-Server-IIS")]
    internal sealed class IISEventSource : EventSource
    {
        public static readonly IISEventSource Log = new IISEventSource();

        private IISEventSource()
        {
        }

        [NonEvent]
        public void RequestStart(IISHttpContext httpContext)
        {
            // avoid allocating the trace identifier unless logging is enabled
            if (IsEnabled())
            {
                RequestStart(httpContext.ConnectionId.ToString(), httpContext.TraceIdentifier);
            }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        [Event(3, Level = EventLevel.Verbose)]
        private void RequestStart(string connectionId, string requestId)
        {
            WriteEvent(3, connectionId, requestId);
        }

        [NonEvent]
        public void RequestStop(IISHttpContext httpContext)
        {
            // avoid allocating the trace identifier unless logging is enabled
            if (IsEnabled())
            {
                RequestStop(httpContext.ConnectionId.ToString(), httpContext.TraceIdentifier);
            }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        [Event(4, Level = EventLevel.Verbose)]
        private void RequestStop(string connectionId, string requestId)
        {
            WriteEvent(4, connectionId, requestId);
        }
    }
}
