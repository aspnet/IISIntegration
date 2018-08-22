using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Http.Features;
using Microsoft.AspNetCore.Server.IISIntegration;
using Microsoft.AspNetCore.Server.Kestrel.Core.Adapter.Internal;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace IISSample
{
    public class Startup
    {
        public void ConfigureServices(IServiceCollection services)
        {
            // These two middleware are registered via an IStartupFilter in UseIISIntegration but you can configure them here.
            //services.Configure<IISOptions>(options =>
            //{
            //    options.AuthenticationDisplayName = "Windows Auth";
            //});
            //services.Configure<ForwardedHeadersOptions>(options =>
            //{
            //});
        }

        public void Configure(IApplicationBuilder app, ILoggerFactory loggerfactory/*, IAuthenticationSchemeProvider authSchemeProvider*/)
        {
            var logger = loggerfactory.CreateLogger("Requests");

            app.Run(async (context) =>
            {
                //context.Response.Headers["Name"] = "`";
                context.Response.Headers["Message"] = "~";

                logger.LogDebug("Received request: " + context.Request.Method + " " + context.Request.Path);

                context.Response.ContentType = "text/plain";
                await context.Response.WriteAsync("Hello World - " + DateTimeOffset.Now + Environment.NewLine);
                await context.Response.WriteAsync(Environment.NewLine);

                await context.Response.WriteAsync("Address:" + Environment.NewLine);
                await context.Response.WriteAsync("Scheme: " + context.Request.Scheme + Environment.NewLine);
                await context.Response.WriteAsync("Host: " + context.Request.Headers["Host"] + Environment.NewLine);
                await context.Response.WriteAsync("PathBase: " + context.Request.PathBase.Value + Environment.NewLine);
                await context.Response.WriteAsync("Path: " + context.Request.Path.Value + Environment.NewLine);
                await context.Response.WriteAsync("Query: " + context.Request.QueryString.Value + Environment.NewLine);
                await context.Response.WriteAsync(Environment.NewLine);

                await context.Response.WriteAsync("Connection:" + Environment.NewLine);
                await context.Response.WriteAsync("RemoteIp: " + context.Connection.RemoteIpAddress + Environment.NewLine);
                await context.Response.WriteAsync("RemotePort: " + context.Connection.RemotePort + Environment.NewLine);
                await context.Response.WriteAsync("LocalIp: " + context.Connection.LocalIpAddress + Environment.NewLine);
                await context.Response.WriteAsync("LocalPort: " + context.Connection.LocalPort + Environment.NewLine);
                await context.Response.WriteAsync("ClientCert: " + context.Connection.ClientCertificate + Environment.NewLine);
                await context.Response.WriteAsync(Environment.NewLine);

                await context.Response.WriteAsync("User: " + context.User.Identity.Name + Environment.NewLine);
                //var scheme = await authSchemeProvider.GetSchemeAsync(IISDefaults.AuthenticationScheme);
                //await context.Response.WriteAsync("DisplayName: " + scheme?.DisplayName + Environment.NewLine);
                await context.Response.WriteAsync(Environment.NewLine);

                await context.Response.WriteAsync("Headers:" + Environment.NewLine);
                foreach (var header in context.Request.Headers)
                {
                    await context.Response.WriteAsync(header.Key + ": " + header.Value + Environment.NewLine);
                }
                await context.Response.WriteAsync(Environment.NewLine);

                await context.Response.WriteAsync("Environment Variables:" + Environment.NewLine);
                var vars = Environment.GetEnvironmentVariables();
                foreach (var key in vars.Keys.Cast<string>().OrderBy(key => key, StringComparer.OrdinalIgnoreCase))
                {
                    var value = vars[key];
                    await context.Response.WriteAsync(key + ": " + value + Environment.NewLine);
                }

                await context.Response.WriteAsync(Environment.NewLine);
                if (context.Features.Get<IHttpUpgradeFeature>() != null)
                {
                    await context.Response.WriteAsync("Websocket feature is enabled.");
                }
                else
                {
                    await context.Response.WriteAsync("Websocket feature is disabled.");
                }
            });
        }

        public static void Main(string[] args)
        {
            var host = new WebHostBuilder()
                .ConfigureLogging((_, factory) =>
                {
                    factory.AddConsole();
                    factory.SetMinimumLevel(LogLevel.Trace);
                })
                .UseKestrel(options =>
                {
                    //options.Listen(new IPEndPoint(IPAddress.Loopback, 5000), listenOptions =>
                    //{
                    //    listenOptions.UseConnectionLogging();
                    //});
                    options.ConfigureEndpointDefaults(listenOptions =>
                    {
                        listenOptions.UseConnectionLogging();
                        listenOptions.ConnectionAdapters.Add(new RawHeaderAdapter());
                    });
                })
                .UseStartup<Startup>()
                .Build();

            host.Run();
        }

        public class RawHeaderAdapter : IConnectionAdapter
        {
            public bool IsHttps => false;

            public Task<IAdaptedConnection> OnConnectionAsync(ConnectionAdapterContext context)
            {
                return Task.FromResult<IAdaptedConnection>(new RawHeaderAdaptedConnection(context));
            }
        }

        public class RawHeaderAdaptedConnection : IAdaptedConnection
        {
            private Stream RawHeaderStream;

            public RawHeaderAdaptedConnection(ConnectionAdapterContext context)
            {
                RawHeaderStream = new RawHeaderStream(context.ConnectionStream);
            }

            public Stream ConnectionStream => RawHeaderStream;

            public void Dispose() { }
        }

        public class RawHeaderStream : Stream
        {
            private Stream _connectionStream;
            private byte[] ASCII = Encoding.GetEncoding("ISO-8859-1").GetBytes("François");
            private byte[] UTF8 = Encoding.UTF8.GetBytes("François");

            public RawHeaderStream(Stream connectionStream)
            {
                _connectionStream = connectionStream;
            }

            public override bool CanRead => _connectionStream.CanRead;

            public override bool CanSeek => _connectionStream.CanSeek;

            public override bool CanWrite => _connectionStream.CanWrite;

            public override long Length => _connectionStream.Length;

            public override long Position
            {
                get
                {
                    return _connectionStream.Position;
                }
                set
                {
                    _connectionStream.Position = value;
                }
            }


            public override void Flush()
            {
                _connectionStream.Flush();
            }

            public override int Read(byte[] buffer, int offset, int count)
            {
                return _connectionStream.Read(buffer, offset, count);
            }

            public override long Seek(long offset, SeekOrigin origin)
            {
                return _connectionStream.Seek(offset, origin);
            }

            public override void SetLength(long value)
            {
                _connectionStream.SetLength(value);
            }

            public override void Write(byte[] buffer, int offset, int count)
            {
                for (int i = 0; i < buffer.Length; i++)
                {
                    if (buffer[i] == '*')
                    {
                        buffer[i] = (byte)'!';
                    }
                }

                _connectionStream.Write(buffer, offset, count);
            }

            public override Task WriteAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
            {
                for (int i = offset; i < offset + count; i++)
                {
                    if (buffer[i] == '`')
                    {
                        _connectionStream.Write(ASCII, 0, ASCII.Length);
                    }
                    else if (buffer[i] == '~')
                    {
                        _connectionStream.Write(UTF8, 0, UTF8.Length);
                    }
                    else
                    {
                        _connectionStream.WriteByte(buffer[i]);
                    }
                }
                return Task.CompletedTask;
            }
        }
    }
}

