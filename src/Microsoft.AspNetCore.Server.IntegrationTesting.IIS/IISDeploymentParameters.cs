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
        private IList<Action<XElement>> _webConfigActionList = new List<Action<XElement>>();
        private IList<Action<XElement>> _serverConfigActionList = new List<Action<XElement>>();

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

        public void RunWebConfigActions()
        {
            if (!PublishApplicationBeforeDeployment)
            {
                // If we aren't publishing, don't modify the webconfig file as 
                throw new InvalidOperationException("Cannot modify web.config file if no published output.");
            }

            var path = Path.Combine(PublishedApplicationRootPath, "web.config");
            var webconfig = XDocument.Load(path);
            var xElement = webconfig.Descendants("aspNetCore").Single();
            foreach (var action in _webConfigActionList)
            {
                action.Invoke(xElement);
            }

            webconfig.Save(path);
        }

        public string RunServerConfigActions(string serverConfig)
        {
            var element = XDocument.Parse(serverConfig);
            foreach (var action in _serverConfigActionList)
            {
                action.Invoke(element.Element("configuration"));
            }
            return element.ToString();
        }

        public void AddHttpsToServerConfig()
        {
            _serverConfigActionList.Add(
                element => {
                    element.Descendants("binding")
                        .Single()
                        .SetAttributeValue("protocol", "https");

                    element.Descendants("access")
                        .Single()
                        .SetAttributeValue("sslFlags", "Ssl, SslNegotiateCert");
                });
        }

        public void AddWindowsAuthToServerConfig()
        {
            _serverConfigActionList.Add(
                element => {
                    element.Descendants("windowsAuthentication")
                        .Single()
                        .SetAttributeValue("enabled", "true");
                });
        }

        public void AddServerConfigAction(Action<XElement> action)
        {
            _serverConfigActionList.Add(action);
        }

        public void ModifyWebConfig(Action<XElement> transform)
        {
            _webConfigActionList.Add(transform);
        }

        public void ModifyAspNetCoreSectionInWebConfig(string key, string value)
            => ModifyAttributeInWebConfig(key, value, section: "aspNetCore");


        public void ModifyAttributeInWebConfig(string key, string value, string section)
        {
            _webConfigActionList.Add((element) =>
            {
                element.SetAttributeValue(key, value);
            });
        }

        public void AddDebugLogToWebConfig(string filename)
        {
            _webConfigActionList.Add(xElement =>
            {
                var element = xElement.Descendants("handlerSettings").SingleOrDefault();
                if (element == null)
                {
                    element = new XElement("handlerSettings");
                    xElement.Add(element);
                }

                CreateOrSetElement(element, "debugLevel", "4", "handlerSetting");

                CreateOrSetElement(element, "debugFile", Path.Combine(PublishedApplicationRootPath, filename), "handlerSetting");
            });
        }

        public void AddEnvironmentVariablesToWebConfig(IDictionary<string, string> environmentVariables)
        {
            _webConfigActionList.Add(xElement =>
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

        public void ModifyHandlerSectionInWebConfig(string handlerVersionValue)
        {
            _webConfigActionList.Add((element) =>
            {
                var handlerVersionElement = new XElement("handlerSetting");
                handlerVersionElement.SetAttributeValue("name", "handlerVersion");
                handlerVersionElement.SetAttributeValue("value", handlerVersionValue);

                element.Add(new XElement("handlerSettings", handlerVersionElement));
            });
        }

        private void CreateOrSetElement(XElement rootElement, string name, string value, string elementName)
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
