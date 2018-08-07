// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Server.IIS.FunctionalTests.Utilities;
using Microsoft.AspNetCore.Server.IntegrationTesting.IIS;
using Microsoft.AspNetCore.Testing.xunit;
using Xunit;

namespace Microsoft.AspNetCore.Server.IISIntegration.FunctionalTests
{
    [Collection(PublishedSitesCollection.Name)]
    public class StartupExceptionTests : IISFunctionalTestBase
    {
        private readonly PublishedSitesFixture _fixture;

        public StartupExceptionTests(PublishedSitesFixture fixture)
        {
            _fixture = fixture;
        }

        [ConditionalTheory]
        [InlineData("CheckLogFile")]
        [InlineData("CheckErrLogFile")]
        public async Task CheckStdoutWithRandomNumber(string path)
        {
            // Forcing publish for now to have parity between IIS and IISExpress
            // Reason is because by default for IISExpress, we expect there to not be a web.config file.
            // However, for IIS, we need a web.config file because the default on generated on publish
            // doesn't include V2. We can remove the publish flag once IIS supports non-publish running
            var deploymentParameters = _fixture.GetBaseDeploymentParameters(_fixture.StartupExceptionWebsite, publish: true);

            var randomNumberString = new Random(Guid.NewGuid().GetHashCode()).Next(10000000).ToString();
            deploymentParameters.WebConfigBasedEnvironmentVariables["ASPNETCORE_INPROCESS_STARTUP_VALUE"] = path;
            deploymentParameters.WebConfigBasedEnvironmentVariables["ASPNETCORE_INPROCESS_RANDOM_VALUE"] = randomNumberString;

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.RetryingHttpClient.GetAsync(path);

            Assert.Equal(HttpStatusCode.InternalServerError, response.StatusCode);

            StopServer();

            Assert.Contains(TestSink.Writes, context => context.Message.Contains($"Random number: {randomNumberString}"));
        }

        [ConditionalTheory]
        [InlineData("CheckLargeStdErrWrites")]
        [InlineData("CheckLargeStdOutWrites")]
        [InlineData("CheckOversizedStdErrWrites")]
        [InlineData("CheckOversizedStdOutWrites")]
        public async Task CheckStdoutWithLargeWrites(string path)
        {
            var deploymentParameters = _fixture.GetBaseDeploymentParameters(_fixture.StartupExceptionWebsite, publish: true);
            deploymentParameters.WebConfigBasedEnvironmentVariables["ASPNETCORE_INPROCESS_STARTUP_VALUE"] = path;

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.RetryingHttpClient.GetAsync(path);

            Assert.Equal(HttpStatusCode.InternalServerError, response.StatusCode);

            StopServer();

            Assert.Contains(TestSink.Writes, context => context.Message.Contains(new string('a', 4096)));
        }

        [ConditionalFact]
        public async Task Gets500_30_ErrorPage()
        {
            var deploymentParameters = _fixture.GetBaseDeploymentParameters(_fixture.StartupExceptionWebsite, publish: true);

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);
            Assert.Equal(HttpStatusCode.InternalServerError, response.StatusCode);

            var responseText = await response.Content.ReadAsStringAsync();
            Assert.Contains("500.30 - ANCM In-Process Start Failure", responseText);
        }

        [ConditionalFact]
        public async Task FrameworkNotFoundExceptionLogged_Pipe()
        {
            var deploymentParameters = GetStartupExceptionParameters();

            var deploymentResult = await DeployAsync(deploymentParameters);

            InvalidateRuntimeConfig(deploymentResult);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();

            EventLogHelpers.VerifyEventLogEvent(TestSink, "Could not find the assembly 'aspnetcorev2_inprocess.dll' for in-process application.");
            Assert.Contains(TestSink.Writes, context => context.Message.Contains("The specified framework 'Microsoft.NETCore.App', version '2.9.9' was not found."));
        }

        [ConditionalFact]
        public async Task FrameworkNotFoundExceptionLogged_File()
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.GracefulShutdown = true;

            var pathToLogs = deploymentParameters.EnableLogging();

            var deploymentResult = await DeployAsync(deploymentParameters);

            InvalidateRuntimeConfig(deploymentResult);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();

            var fileInDirectory = Directory.GetFiles(pathToLogs).Single();
            var contents = File.ReadAllText(fileInDirectory);

            Assert.Contains("The specified framework 'Microsoft.NETCore.App', version '2.9.9' was not found.", contents);
        }

        [ConditionalFact]
        public async Task EnableCoreHostTraceLogging_TwoLogFilesCreated()
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.EnvironmentVariables["COREHOST_TRACE"] = "1";
            deploymentParameters.GracefulShutdown = true;

            var pathToLogs = deploymentParameters.EnableLogging();

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();

            var filesInDirectory = Directory.GetFiles(pathToLogs);
            Assert.Equal(2, filesInDirectory.Length);
        }

        [ConditionalTheory]
        [InlineData("CheckLargeStdErrWrites")]
        [InlineData("CheckLargeStdOutWrites")]
        [InlineData("CheckOversizedStdErrWrites")]
        [InlineData("CheckOversizedStdOutWrites")]
        public async Task EnableCoreHostTraceLogging_PipeRestoreCorrectly(string path)
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.EnvironmentVariables["COREHOST_TRACE"] = "1";
            deploymentParameters.WebConfigBasedEnvironmentVariables["ASPNETCORE_INPROCESS_STARTUP_VALUE"] = path;

            deploymentParameters.GracefulShutdown = true;

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");

            Assert.False(response.IsSuccessStatusCode);

            StopServer();
            Assert.Contains(TestSink.Writes, context => context.Message.Contains("Invoked hostfxr"));
        }

        [ConditionalTheory]
        [InlineData("CheckLargeStdErrWrites")]
        [InlineData("CheckLargeStdOutWrites")]
        [InlineData("CheckOversizedStdErrWrites")]
        [InlineData("CheckOversizedStdOutWrites")]
        public async Task EnableCoreHostTraceLogging_FileRestoreCorrectly(string path)
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.EnvironmentVariables["COREHOST_TRACE"] = "1";
            deploymentParameters.WebConfigBasedEnvironmentVariables["ASPNETCORE_INPROCESS_STARTUP_VALUE"] = path;
            var pathToLogs = deploymentParameters.EnableLogging();
            deploymentParameters.GracefulShutdown = true;

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();

            var fileInDirectory = Directory.GetFiles(pathToLogs).First();
            var contents = File.ReadAllText(fileInDirectory);

            Assert.Contains("Invoked hostfxr", contents);

            //fileInDirectory = Directory.GetFiles(pathToLogs).Last();

            //contents = File.ReadAllText(fileInDirectory);
            //Assert.Contains("Invoked hostfxr", contents);

            Assert.DoesNotContain(TestSink.Writes, context => context.Message.Contains("breadcrumb"));
        }

        private static IISDeploymentParameters GetStartupExceptionParameters()
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.GracefulShutdown = true;
            return deploymentParameters;
        }

        private static void InvalidateRuntimeConfig(IISDeploymentResult deploymentResult)
        {
            var path = Path.Combine(deploymentResult.ContentRoot, "StartupExceptionWebsite.runtimeconfig.json");
            var depsFileContent = File.ReadAllText(path);
            depsFileContent = depsFileContent.Replace("2.2.0-preview1-26618-02", "2.9.9");
            File.WriteAllText(path, depsFileContent);
        }
    }
}
