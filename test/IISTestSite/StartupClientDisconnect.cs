// Copyright (c) .NET Foundation. All rights reserved.
// Licensed under the Apache License, Version 2.0. See License.txt in the project root for license information.

using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Logging;
using Xunit;

namespace IISTestSite
{
    public class StartupClientDisconnect
    {
        private ManualResetEvent _cancelled = new ManualResetEvent(false);
        public void Configure(IApplicationBuilder app, ILoggerFactory loggerFactory)
        {
            app.Run(async ctx =>
            {
                if (ctx.Request.Path.StartsWithSegments("/ClientDisconnect"))
                {
                    // Make it a very large content length as we want to trigger disconnect
                    await ctx.Response.WriteAsync("ClientDisconnect");
                    ctx.RequestAborted.Register(() =>
                    {
                        _cancelled.Set();
                    });

                    var waitTime = Debugger.IsAttached ? 10000000 : 1000;
                    await Task.Run(() =>
                    {
                        Assert.True(_cancelled.WaitOne(waitTime));
                    });

                }
                else if (ctx.Request.Path.StartsWithSegments("/Results"))
                {
                    var waitTime = Debugger.IsAttached ? 10000000 : 1000;
                    if (_cancelled.WaitOne(waitTime))
                    {
                        await ctx.Response.WriteAsync("Success");
                    }
                    else
                    {
                        await ctx.Response.WriteAsync("Failure");
                    }
                }
                else if (ctx.Request.Path.StartsWithSegments("/Abort"))
                {
                    ctx.Abort();
                }
                else
                {
                    await ctx.Response.WriteAsync("Hello World");
                }
            });
        }
    }
}
