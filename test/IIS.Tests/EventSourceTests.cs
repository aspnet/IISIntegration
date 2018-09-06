// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics.Tracing;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Server.IIS.Core;
using Microsoft.AspNetCore.Testing.xunit;
using Microsoft.Extensions.Logging.Testing;
using Xunit;

namespace Microsoft.AspNetCore.Server.IISIntegration.FunctionalTests
{
    public class EventSourceTests : LoggedTest
    {
        private readonly TestEventListener _listener = new TestEventListener();

        public EventSourceTests()
        {
            _listener.EnableEvents(IISEventSource.Log, EventLevel.Verbose);
        }


        [ConditionalFact]
        public void ExistsWithCorrectId()
        {
            var esType = typeof(IISHttpServer).GetTypeInfo().Assembly.GetType(
                "Microsoft.AspNetCore.Server.IIS.Core.IISEventSource",
                throwOnError: true,
                ignoreCase: false
            );

            Assert.NotNull(esType);

            Assert.Equal("Microsoft-AspNetCore-Server-IIS", EventSource.GetName(esType));
            Assert.Equal(Guid.Parse("704daae7-8885-5145-f681-83b081088519"), EventSource.GetGuid(esType));
            Assert.NotEmpty(EventSource.GenerateManifest(esType, "assemblyPathToIncludeInManifest"));
        }

        [ConditionalFact]
        public async Task EmitsRequestStartAndStop()
        {
            var helloWorld = "Hello World";
            var expectedPath = "/Path";
            string requestId = null;
            string path = null;

            using (var testServer = await TestServer.Create(ctx =>
            {
                path = ctx.Request.Path.ToString();
                requestId = ctx.TraceIdentifier;
                return ctx.Response.WriteAsync(helloWorld);
            }, LoggerFactory))
            {
                var result = await testServer.HttpClient.GetAsync(expectedPath);
                Assert.Equal(helloWorld, await result.Content.ReadAsStringAsync());
                Assert.Equal(expectedPath, path);
            }

            // capture list here as other tests executing in parallel may log events
            Assert.NotNull(requestId);

            var events = _listener.EventData.Where(e => e != null && GetProperty(e, "requestId") == requestId).ToList();
            {
                var requestStart = Assert.Single(events, e => e.EventName == "RequestStart");
                Assert.All(new[] { "connectionId", "requestId" }, p => Assert.Contains(p, requestStart.PayloadNames));
                Assert.Equal(requestId, GetProperty(requestStart, "requestId"));
                Assert.Same(IISEventSource.Log, requestStart.EventSource);
            }
            {
                var requestStop = Assert.Single(events, e => e.EventName == "RequestStop");
                Assert.All(new[] { "connectionId", "requestId" }, p => Assert.Contains(p, requestStop.PayloadNames));
                Assert.Equal(requestId, GetProperty(requestStop, "requestId"));
                Assert.Same(IISEventSource.Log, requestStop.EventSource);
            }
        }

        private string GetProperty(EventWrittenEventArgs data, string propName)
            => data.Payload[data.PayloadNames.IndexOf(propName)] as string;

        private class TestEventListener : EventListener
        {
            private volatile bool _disposed;
            private ConcurrentQueue<EventWrittenEventArgs> _events = new ConcurrentQueue<EventWrittenEventArgs>();

            public IEnumerable<EventWrittenEventArgs> EventData => _events;

            protected override void OnEventWritten(EventWrittenEventArgs eventData)
            {
                if (!_disposed)
                {
                    _events.Enqueue(eventData);
                }
            }

            public override void Dispose()
            {
                _disposed = true;
                base.Dispose();
            }
        }
    }
}
