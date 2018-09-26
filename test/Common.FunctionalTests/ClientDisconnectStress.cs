// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Server.IIS.FunctionalTests.Utilities;
using Microsoft.AspNetCore.Server.IntegrationTesting;
using Microsoft.AspNetCore.Testing.xunit;
using Xunit;
using Microsoft.AspNetCore.Server.IntegrationTesting.IIS;
using System.IO;
using Microsoft.Extensions.Logging;

namespace Microsoft.AspNetCore.Server.IISIntegration.FunctionalTests
{
    [Collection(PublishedSitesCollection.Name)]
    public class ClientDisconnectStressTests: LogFileTestBase
    {
        private readonly PublishedSitesFixture _fixture;

        public ClientDisconnectStressTests(PublishedSitesFixture fixture)
        {
            _fixture = fixture;
        }

        [ConditionalTheory]
        [InlineData(HostingModel.InProcess)]
        [InlineData(HostingModel.OutOfProcess)]
        public async Task ClientDisconnectStress(HostingModel hostingModel)
        {
            var parameters = _fixture.GetBaseDeploymentParameters(hostingModel);

            var site = await StartAsync(parameters);
            var maxRequestSize = 1000;
            var blockSize = 40;
            var random = new Random();
            async Task RunRequests()
            {
                using (var connection = new TestConnection(site.HttpClient.BaseAddress.Port))
                {
                    await connection.Send(
                        "POST /ReadAndFlushEcho HTTP/1.1",
                        $"Content-Length: {maxRequestSize}",
                        "Host: localhost",
                        "",
                        "");

                    var disconnectAfter = random.Next(maxRequestSize);
                    var data = new byte[blockSize];
                    for (int i = 0; i < disconnectAfter / blockSize; i++)
                    {
                        await connection.Stream.WriteAsync(data);
                    }
                }
            }

            List<Task> tasks = new List<Task>();
            for (int i = 0; i < 1000; i++)
            {
                tasks.Add(Task.Run(RunRequests));
            }

            await Task.WhenAll(tasks);

            StopServer();
        }
    }
}
