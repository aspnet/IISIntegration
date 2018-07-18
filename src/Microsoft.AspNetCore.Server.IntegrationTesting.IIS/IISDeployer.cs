// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Server.IntegrationTesting.Common;
using Microsoft.Extensions.Logging;

namespace Microsoft.AspNetCore.Server.IntegrationTesting.IIS
{
    /// <summary>
    /// Deployer for IIS.
    /// </summary>
    public partial class IISDeployer : IISDeployerBase
    {
        private IISApplication _application;
        private CancellationTokenSource _hostShutdownToken = new CancellationTokenSource();

        public IISDeployer(DeploymentParameters deploymentParameters, ILoggerFactory loggerFactory)
            : base(deploymentParameters, loggerFactory)
        {
        }

        public IISDeployer(IISDeploymentParameters deploymentParameters, ILoggerFactory loggerFactory)
            : base(deploymentParameters, loggerFactory)
        {
        }

        public override void Dispose()
        {
            if (_application != null)
            {
                _application.StopAndDeleteAppPool().GetAwaiter().GetResult();

                TriggerHostShutdown(_hostShutdownToken);
            }

            GetLogsFromFile($"{_application.WebSiteName}.txt");

            CleanPublishedOutput();
            InvokeUserApplicationCleanup();

            StopTimer();
        }

        public override async Task<DeploymentResult> DeployAsync()
        {
            using (Logger.BeginScope("Deployment"))
            {
                StartTimer();

                var contentRoot = string.Empty;
                if (string.IsNullOrEmpty(IISDeploymentParameters.ServerConfigTemplateContent))
                {
                    IISDeploymentParameters.ServerConfigTemplateContent = File.ReadAllText("IIS.config");
                }

                _application = new IISApplication(IISDeploymentParameters, Logger);

                // For now, only support using published output
                IISDeploymentParameters.PublishApplicationBeforeDeployment = true;

                if (IISDeploymentParameters.PublishApplicationBeforeDeployment)
                {
                    DotnetPublish();
                    contentRoot = IISDeploymentParameters.PublishedApplicationRootPath;
                    IISDeploymentParameters.AddDebugLogToWebConfig(Path.Combine(contentRoot, $"{_application.WebSiteName}.txt"));
                    RunWebConfigActions();
                }

                var uri = TestUriHelper.BuildTestUri(ServerType.IIS, IISDeploymentParameters.ApplicationBaseUriHint);
                // To prevent modifying the IIS setup concurrently.
                await _application.StartIIS(uri, contentRoot);

                // Warm up time for IIS setup.
                Logger.LogInformation("Successfully finished IIS application directory setup.");

                return new DeploymentResult(
                    LoggerFactory,
                    IISDeploymentParameters,
                    applicationBaseUri: uri.ToString(),
                    contentRoot: contentRoot,
                    hostShutdownToken: _hostShutdownToken.Token
                );
            }
        }

        private void GetLogsFromFile(string file)
        {
            var arr = new string[0];

            RetryHelper.RetryOperation(() => arr = File.ReadAllLines(Path.Combine(IISDeploymentParameters.PublishedApplicationRootPath, file)),
                            (ex) => Logger.LogWarning("Could not read log file"),
                            5,
                            200);

            Logger.LogInformation($"Found debug log file: {file}");
            foreach (var line in arr)
            {
                Logger.LogInformation(line);
            }
        }
    }
}
