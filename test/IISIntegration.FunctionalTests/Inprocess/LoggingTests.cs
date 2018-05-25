// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System.IO;
using System.Linq;
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
            var deploymentParameters = Helpers.GetBaseDeploymentParameters();

            var deploymentResult = await DeployAsync(deploymentParameters);

            Helpers.ModifyAspNetCoreSectionInWebConfig(deploymentResult, "stdoutLogEnabled", "true");
            Helpers.ModifyAspNetCoreSectionInWebConfig(deploymentResult, "stdoutLogFile", @".\logs\stdout");

            var response = await deploymentResult.RetryingHttpClient.GetAsync(path);

            var responseText = await response.Content.ReadAsStringAsync();

            Assert.Equal("Hello World", responseText);

            Dispose();

            var fileInDirectory = Directory.GetFiles(Path.Combine(deploymentResult.DeploymentResult.ContentRoot, @"\logs\")).Single();
            Assert.NotNull(fileInDirectory);

            string contents = null;
            for (var i = 0; i < 20; i++)
            {
                try
                {
                    contents = await File.ReadAllTextAsync(fileInDirectory);
                    break;
                }
                catch (IOException)
                {
                    // File in use by IISExpress, delay a bit before rereading.
                }
                await Task.Delay(20);
            }

            Assert.NotNull(contents);

            // Open the log file and see if there are any contents.
            Assert.Contains("TEST MESSAGE", contents);
        }
    }
}
