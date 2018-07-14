// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Linq;
using System.Threading.Tasks;
using System.Xml.Linq;
using Microsoft.AspNetCore.Server.IISIntegration.FunctionalTests;
using Microsoft.AspNetCore.Server.IntegrationTesting;
using Xunit.Abstractions;

namespace Microsoft.AspNetCore.Server.IIS.FunctionalTests.Utilities
{
    public class IISFunctionalTestBase : FunctionalTestsBase
    {
        public IISFunctionalTestBase(ITestOutputHelper output = null) : base(output)
        {
        }
    }
}
