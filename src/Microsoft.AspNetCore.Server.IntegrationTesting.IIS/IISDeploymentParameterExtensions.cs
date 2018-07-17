// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace Microsoft.AspNetCore.Server.IntegrationTesting.IIS
{
    public static class IISDeploymentParameterExtensions
    {
        public static void AddDebugLogToWebConfig(this IISDeploymentParameters parameters, string filename)
        {
            parameters.WebConfigActionList.Add(xElement =>
            {
                var element = xElement.Descendants("handlerSettings").SingleOrDefault();
                if (element == null)
                {
                    element = new XElement("handlerSettings");
                    xElement.Add(element);
                }

                CreateOrSetElement(element, "debugLevel", "4", "handlerSetting");

                CreateOrSetElement(element, "debugFile", Path.Combine(parameters.PublishedApplicationRootPath, filename), "handlerSetting");
            });
        }

        public static void AddEnvironmentVariablesToWebConfig(this IISDeploymentParameters parameters, string key, string value)
        {
            parameters.WebConfigActionList.Add(xElement =>
            {
                var element = xElement.Descendants("environmentVariables").SingleOrDefault();
                if (element == null)
                {
                    element = new XElement("environmentVariables");
                    xElement.Add(element);
                }

                CreateOrSetElement(element, key, value, "environmentVariable");
            });
        }

        public static void AddEnvironmentVariablesToWebConfig(this IISDeploymentParameters parameters, IDictionary<string, string> environmentVariables)
        {
            parameters.WebConfigActionList.Add(xElement =>
            {
                var element = xElement.Descendants("environmentVariables").SingleOrDefault();
                if (element == null)
                {
                    element = new XElement("environmentVariables");
                    xElement.Add(element);
                }

                foreach (var envVar in environmentVariables)
                {
                    CreateOrSetElement(element, envVar.Key, envVar.Value, "environmentVariable");
                }
            });
        }

        public static void AddServerConfigAction(this IISDeploymentParameters parameters, Action<XElement> action)
        {
            parameters.ServerConfigActionList.Add(action);
        }

        public static void ModifyWebConfig(this IISDeploymentParameters parameters, Action<XElement> transform)
        {
            parameters.WebConfigActionList.Add(transform);
        }

        public static void ModifyAspNetCoreSectionInWebConfig(this IISDeploymentParameters parameters, string key, string value)
            => ModifyAttributeInWebConfig(parameters, key, value, section: "aspNetCore");


        public static void ModifyAttributeInWebConfig(this IISDeploymentParameters parameters, string key, string value, string section)
        {
            parameters.WebConfigActionList.Add((element) =>
            {
                element.SetAttributeValue(key, value);
            });
        }

        public static void ModifyHandlerSectionInWebConfig(this IISDeploymentParameters parameters, string handlerVersionValue)
        {
            parameters.WebConfigActionList.Add((element) =>
            {
                var handlerVersionElement = new XElement("handlerSetting");
                handlerVersionElement.SetAttributeValue("name", "handlerVersion");
                handlerVersionElement.SetAttributeValue("value", handlerVersionValue);

                element.Add(new XElement("handlerSettings", handlerVersionElement));
            });
        }


        public static void AddHttpsToServerConfig(this IISDeploymentParameters parameters)
        {
            parameters.ServerConfigActionList.Add(
                element =>
                {
                    element.Descendants("binding")
                        .Single()
                        .SetAttributeValue("protocol", "https");

                    element.Descendants("access")
                        .Single()
                        .SetAttributeValue("sslFlags", "Ssl, SslNegotiateCert");
                });
        }

        public static void AddWindowsAuthToServerConfig(this IISDeploymentParameters parameters)
        {
            parameters.ServerConfigActionList.Add(
                element =>
                {
                    element.Descendants("windowsAuthentication")
                        .Single()
                        .SetAttributeValue("enabled", "true");
                });
        }

        private static void CreateOrSetElement(XElement rootElement, string name, string value, string elementName)
        {
            if (rootElement.Descendants()
                .Attributes()
                .Where(attribute => attribute.Value == name)
                .Any())
            {
                return;
            }
            var element = new XElement(elementName);
            element.SetAttributeValue("name", name);
            element.SetAttributeValue("value", value);
            rootElement.Add(element);
        }
    }
}
