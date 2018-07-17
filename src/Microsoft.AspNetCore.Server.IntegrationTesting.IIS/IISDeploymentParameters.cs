// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace Microsoft.AspNetCore.Server.IntegrationTesting.IIS
{
    public class IISDeploymentParameters : DeploymentParameters
    {


        public IISDeploymentParameters() : base()
        {
        }

        public IISDeploymentParameters(TestVariant variant)
            : base(variant)
        {
        }

        public IISDeploymentParameters(
           string applicationPath,
           ServerType serverType,
           RuntimeFlavor runtimeFlavor,
           RuntimeArchitecture runtimeArchitecture)
            : base (applicationPath, serverType, runtimeFlavor, runtimeArchitecture)
        {
        }

        public IList<Action<XElement>> WebConfigActionList { get; } = new List<Action<XElement>>();
        public IList<Action<XElement>> ServerConfigActionList { get; } = new List<Action<XElement>>();

        public IDictionary<string, string> WebConfigBasedEnvironmentVariables { get; set; }

        public IDictionary<string, string> HandlerSettings { get; set; }

    }
}
