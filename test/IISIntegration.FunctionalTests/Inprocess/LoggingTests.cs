// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System.IO;
using System.Threading.Tasks;
using IISIntegration.FunctionalTests.Utilities;
using Microsoft.AspNetCore.Server.IntegrationTesting;
using Xunit;

namespace Microsoft.AspNetCore.Server.IISIntegration.FunctionalTests
{
    public class LoggingTests : IISFunctionalTestBase
    {
        [Theory]
        [InlineData("CheckErrLogFile", Skip = "StdErr logs are now showing up in log file.")]
        [InlineData("CheckLogFile")]
        public async Task CheckStdoutLogging(string path)
        {
            var deploymentParameters = GetBaseDeploymentParameters();

            // Point to dotnet installed in user profile.
            var deploymentResult = await DeployAsync(deploymentParameters);

            Helpers.ModifyAspNetCoreSectionInWebConfig(deploymentResult, "stdoutLogEnabled", "true");
            Helpers.ModifyAspNetCoreSectionInWebConfig(deploymentResult, "stdoutLogFile", @".\logs\stdout");

            // Request to base address and check if various parts of the body are rendered & measure the cold startup time.

            var response = await deploymentResult.RetryingHttpClient.GetAsync(path);

            var responseText = await response.Content.ReadAsStringAsync();
            Assert.Equal("Hello World", responseText);

            Dispose();
            var filesInDirectory = Directory.GetFiles($@"{deploymentResult.DeploymentResult.ContentRoot}\logs\");
            Assert.NotEmpty(filesInDirectory);
            FileStream fileStream = null;
            for (var i = 0; i < 20; i++)
            {
                try
                {
                    fileStream = File.Open(filesInDirectory[0], FileMode.Open);
                    break;
                }
                catch (IOException)
                {
                    // File in use by IISExpress, delay a bit before rereading.
                }
                await Task.Delay(20);
            }
            Assert.NotNull(fileStream);

            // Open the log file and see if there are any contents.
            var fileReader = new StreamReader(fileStream);
            var contents = fileReader.ReadToEnd();
            Assert.Contains("TEST MESSAGE", contents);
        }

        private DeploymentParameters GetBaseDeploymentParameters(string site = null)
        {
            return new DeploymentParameters(Helpers.GetTestWebSitePath(site ?? "InProcessWebSite"), ServerType.IISExpress, RuntimeFlavor.CoreClr, RuntimeArchitecture.x64)
            {
                TargetFramework = Tfm.NetCoreApp22,
                ApplicationType = ApplicationType.Portable,
                AncmVersion = AncmVersion.AspNetCoreModuleV2
            };
        }
    }
}
