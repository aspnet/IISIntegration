// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Xml.Linq;
using Microsoft.Extensions.Logging;

namespace Microsoft.AspNetCore.Server.IntegrationTesting.IIS
{
    public class IISDeployerBase : ApplicationDeployer
    {
        public IISDeploymentParameters IISDeploymentParameters { get; }

        public IISDeployerBase(DeploymentParameters deploymentParameters, ILoggerFactory loggerFactory)
            : base(deploymentParameters, loggerFactory)
        {
        }

        public IISDeployerBase(IISDeploymentParameters deploymentParameters, ILoggerFactory loggerFactory)
            : base(deploymentParameters, loggerFactory)
        {
            IISDeploymentParameters = deploymentParameters;
        }
        public void RunWebConfigActions()
        {
            if (!DeploymentParameters.PublishApplicationBeforeDeployment)
            {
                throw new InvalidOperationException("Cannot modify web.config file if no published output.");
            }

            var path = Path.Combine(DeploymentParameters.PublishedApplicationRootPath, "web.config");
            var webconfig = XDocument.Load(path);
            var xElement = webconfig.Descendants("aspNetCore").Single();
            foreach (var action in IISDeploymentParameters.WebConfigActionList)
            {
                action.Invoke(xElement);
            }

            webconfig.Save(path);
        }

        public string RunServerConfigActions(string serverConfig)
        {
            var element = XDocument.Parse(serverConfig);
            foreach (var action in IISDeploymentParameters.ServerConfigActionList)
            {
                action.Invoke(element.Element("configuration"));
            }
            return element.ToString();
        }

        public override Task<DeploymentResult> DeployAsync()
        {
            throw new NotImplementedException("Derived classes must override DeployAsync");
        }

        public override void Dispose()
        {
            throw new NotImplementedException("Derived classes must override Dispose");
        }
    }
}
