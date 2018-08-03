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
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
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

            var response = await deploymentResult.HttpClient.GetAsync(path);

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

            var response = await deploymentResult.HttpClient.GetAsync(path);

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
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);

            var deploymentResult = await DeployAsync(deploymentParameters);

            var path = Path.Combine(deploymentResult.ContentRoot, "StartupExceptionWebsite.runtimeconfig.json");
            var depsFileContent = File.ReadAllText(path);
            depsFileContent = depsFileContent.Replace("2.2.0-preview1-26618-02", "2.9.9");
            File.WriteAllText(path, depsFileContent);

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

            deploymentParameters.WebConfigActionList.Add(
              WebConfigHelpers.AddOrModifyAspNetCoreSection("stdoutLogEnabled", "true"));

            var pathToLogs = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString());
            deploymentParameters.WebConfigActionList.Add(
                WebConfigHelpers.AddOrModifyAspNetCoreSection("stdoutLogFile", Path.Combine(pathToLogs, "std")));

            var deploymentResult = await DeployAsync(deploymentParameters);

            var path = Path.Combine(deploymentResult.ContentRoot, "StartupExceptionWebsite.runtimeconfig.json");
            var depsFileContent = File.ReadAllText(path);
            depsFileContent = depsFileContent.Replace("2.2.0-preview1-26618-02", "2.9.9");
            File.WriteAllText(path, depsFileContent);

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

            deploymentParameters.WebConfigActionList.Add(
                WebConfigHelpers.AddOrModifyAspNetCoreSection("stdoutLogEnabled", "true"));

            var pathToLogs = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString());
            deploymentParameters.WebConfigActionList.Add(
                WebConfigHelpers.AddOrModifyAspNetCoreSection("stdoutLogFile", Path.Combine(pathToLogs, "std")));
            deploymentParameters.EnvironmentVariables["COREHOST_TRACE"] = "1";

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();

            var filesInDirectory = Directory.GetFiles(pathToLogs);
            Assert.Equal(2, filesInDirectory.Length);
        }

        [ConditionalFact]
        public async Task EnableCoreHostTraceLogging_PipeRestoreCorrectly()
        {
            var deploymentParameters = Helpers.GetBaseDeploymentParameters("StartupExceptionWebsite", publish: true);
            deploymentParameters.EnvironmentVariables["COREHOST_TRACE"] = "1";

            var deploymentResult = await DeployAsync(deploymentParameters);

            var response = await deploymentResult.HttpClient.GetAsync("/");
            Assert.False(response.IsSuccessStatusCode);

            StopServer();
            var count = 0;
            foreach (var line in TestSink.Writes)
            {
                if (line.Message.Contains("Tracing enabled"))
                {
                    count++;
                }
            }

            Assert.Equal(2, count);
        }
    }
}
