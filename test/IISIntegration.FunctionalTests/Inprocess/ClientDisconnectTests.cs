// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Server.IntegrationTesting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Testing;
using Xunit;
using Xunit.Abstractions;
using Xunit.Sdk;

namespace Microsoft.AspNetCore.Server.IIS.FunctionalTests
{
    public class ClientDisconnectTests : LoggedTest
    {
        public ClientDisconnectTests(ITestOutputHelper output) : base(output)
        {
        }

        /// <summary>
        /// This test will currently always fail, even though the client disconnect is occuring.
        /// The issue is how we give IIS the ability to handle events. Right now, the only time we
        /// allow the IIS pipleline to handle events is whenever we return RQ_NOTIFICATION_PENDING
        /// which only occurs when async operations are occuring. We cannnot guarantee any async
        /// operations will occur, as it is dependent on if reads or writes complete asynchronously
        /// or synchronously. 
        /// </summary>
        /// <returns></returns>
        [Fact(Skip = "See https://github.com/aspnet/IISIntegration/issues/424")]
        public Task ClientDisconnect_InProcess_IISExpress_CoreClr_X64_Portable()
        {
            return RunClientDisconnect(RuntimeFlavor.CoreClr, ApplicationType.Portable);
        }

        private async Task RunClientDisconnect(RuntimeFlavor runtimeFlavor, ApplicationType applicationType)
        {
            var serverType = ServerType.IISExpress;
            var architecture = RuntimeArchitecture.x64;
            var testName = $"HelloWorld_{runtimeFlavor}";
            using (StartLog(out var loggerFactory, testName))
            {
                var logger = loggerFactory.CreateLogger("ClientDisconnect");

                var deploymentParameters = new DeploymentParameters(Helpers.GetTestSitesPath(), serverType, runtimeFlavor, architecture)
                {
                    EnvironmentName = "ClientDisconnect", // Will pick the Start class named 'StartupHelloWorld',
                    ServerConfigTemplateContent = (serverType == ServerType.IISExpress) ? File.ReadAllText("Http.config") : null,
                    SiteName = "HttpTestSite", // This is configured in the Http.config
                    TargetFramework = runtimeFlavor == RuntimeFlavor.Clr ? "net461" : "netcoreapp2.0",
                    ApplicationType = applicationType,
                    Configuration =
#if DEBUG
                        "Debug"
#else
                        "Release"
#endif
                };

                using (var deployer = ApplicationDeployerFactory.Create(deploymentParameters, loggerFactory))
                {
                    var deploymentResult = await deployer.DeployAsync();
                    deploymentResult.HttpClient.Timeout = TimeSpan.FromSeconds(50);

                    try
                    {
                        var bytesReceived = new byte[1];

                        using (var client = await SendHungRequestAsync("GET", deploymentResult.ApplicationBaseUri + "ClientDisconnect"))
                        {
                            // Read something from the socket to indicate the request has been received.
                            client.Client.Receive(bytesReceived);
                            client.LingerState = new LingerOption(true, 0);
                        }

                        var response = await deploymentResult.HttpClient.GetAsync("/Results");
                        var responseText = await response.Content.ReadAsStringAsync();

                        Assert.Equal("Success", responseText);
                    }
                    catch (XunitException)
                    {
                        throw;
                    }
                }
            }
        }

        private async Task<TcpClient> SendHungRequestAsync(string method, string address)
        {
            var uri = new Uri(address);
            var client = new TcpClient();

            try
            {
                await client.ConnectAsync(uri.Host, uri.Port);
                NetworkStream stream = client.GetStream();

                // Send an HTTP GET request
                byte[] requestBytes = BuildGetRequest(method, uri);
                await stream.WriteAsync(requestBytes, 0, requestBytes.Length);

                return client;
            }
            catch (Exception)
            {
                ((IDisposable)client).Dispose();
                throw;
            }
        }

        private byte[] BuildGetRequest(string method, Uri uri)
        {
            StringBuilder builder = new StringBuilder();
            builder.Append(method);
            builder.Append(" ");
            builder.Append(uri.PathAndQuery);
            builder.Append(" HTTP/1.1");
            builder.AppendLine();

            builder.Append("Host: ");
            builder.Append(uri.Host);
            builder.Append(':');
            builder.Append(uri.Port);
            builder.AppendLine();

            builder.AppendLine();
            return Encoding.ASCII.GetBytes(builder.ToString());
        }
    }
}
