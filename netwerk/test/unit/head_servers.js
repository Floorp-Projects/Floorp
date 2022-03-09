/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_trr.js */

/* globals require, __dirname, global, Buffer, process */

class BaseNodeHTTPServerCode {
  static globalHandler(req, resp) {
    let path = new URL(req.url, "http://example.com").pathname;
    let handler = global.path_handlers[path];
    if (handler) {
      return handler(req, resp);
    }

    // Didn't find a handler for this path.
    let response = `<h1> 404 Path not found: ${path}</h1>`;
    resp.setHeader("Content-Type", "text/html");
    resp.setHeader("Content-Length", response.length);
    resp.writeHead(404);
    resp.end(response);
  }
}

class ADB {
  static async stopForwarding(port) {
    // return this.forwardPort(port, true);
  }

  static async forwardPort(port, remove = false) {
    if (!process.env.MOZ_ANDROID_DATA_DIR) {
      // Not android, or we don't know how to do the forwarding
      return;
    }
    // When creating a server on Android we must make sure that the port
    // is forwarded from the host machine to the emulator.
    let adb_path = "adb";
    if (process.env.MOZ_FETCHES_DIR) {
      adb_path = `${process.env.MOZ_FETCHES_DIR}/android-sdk-linux/platform-tools/adb`;
    }

    let command = `${adb_path} reverse tcp:${port} tcp:${port}`;
    if (remove) {
      command = `${adb_path} reverse --remove tcp:${port}`;
    }

    await new Promise(resolve => {
      const { exec } = require("child_process");
      exec(command, (error, stdout, stderr) => {
        if (error) {
          console.log(`error: ${error.message}`);
          return;
        }
        if (stderr) {
          console.log(`stderr: ${stderr}`);
        }
        // console.log(`stdout: ${stdout}`);
        resolve();
      });
    });
  }
}

class BaseNodeServer {
  protocol() {
    return this._protocol;
  }
  origin() {
    return `${this.protocol()}://localhost:${this.port()}`;
  }
  port() {
    return this._port;
  }

  /// Stops the server
  async stop() {
    if (this.processId) {
      await this.execute(`ADB.stopForwarding(${this.port()})`);
      await NodeServer.kill(this.processId);
      this.processId = undefined;
    }
  }

  /// Executes a command in the context of the node server
  async execute(command) {
    return NodeServer.execute(this.processId, command);
  }

  /// @path : string - the path on the server that we're handling. ex: /path
  /// @handler : function(req, resp, url) - function that processes request and
  ///     emits a response.
  async registerPathHandler(path, handler) {
    return this.execute(
      `global.path_handlers["${path}"] = ${handler.toString()}`
    );
  }
}

// HTTP

class NodeHTTPServerCode extends BaseNodeHTTPServerCode {
  static async startServer(port) {
    const http = require("http");
    global.server = http.createServer(BaseNodeHTTPServerCode.globalHandler);

    await global.server.listen(port);
    let serverPort = global.server.address().port;
    await ADB.forwardPort(serverPort);
    return serverPort;
  }
}

class NodeHTTPServer extends BaseNodeServer {
  _protocol = "http";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTPServerCode);
    await this.execute(ADB);
    this._port = await this.execute(`NodeHTTPServerCode.startServer(${port})`);
    await this.execute(`global.path_handlers = {};`);
  }
}

// HTTPS

class NodeHTTPSServerCode extends BaseNodeHTTPServerCode {
  static async startServer(port) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };
    const https = require("https");
    global.server = https.createServer(
      options,
      BaseNodeHTTPServerCode.globalHandler
    );

    await global.server.listen(port);
    let serverPort = global.server.address().port;
    await ADB.forwardPort(serverPort);
    return serverPort;
  }
}

class NodeHTTPSServer extends BaseNodeServer {
  _protocol = "https";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTPSServerCode);
    await this.execute(ADB);
    this._port = await this.execute(`NodeHTTPSServerCode.startServer(${port})`);
    await this.execute(`global.path_handlers = {};`);
  }
}

// HTTP2

class NodeHTTP2ServerCode extends BaseNodeHTTPServerCode {
  static async startServer(port) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };
    const http2 = require("http2");
    global.server = http2.createSecureServer(
      options,
      BaseNodeHTTPServerCode.globalHandler
    );

    await global.server.listen(port);
    let serverPort = global.server.address().port;
    await ADB.forwardPort(serverPort);
    return serverPort;
  }
}

class NodeHTTP2Server extends BaseNodeServer {
  _protocol = "https";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTP2ServerCode);
    await this.execute(ADB);
    this._port = await this.execute(`NodeHTTP2ServerCode.startServer(${port})`);
    await this.execute(`global.path_handlers = {};`);
  }
}

// Base HTTP proxy

class BaseProxyCode {
  static proxyHandler(req, res) {
    if (req.url.startsWith("/")) {
      res.writeHead(405);
      res.end();
      return;
    }

    const http = require("http");
    http
      .get(req.url, { method: req.method }, proxyresp => {
        res.writeHead(
          proxyresp.statusCode,
          proxyresp.statusMessage,
          proxyresp.headers
        );
        let rawData = "";
        proxyresp.on("data", chunk => {
          rawData += chunk;
        });
        proxyresp.on("end", () => {
          res.end(rawData);
        });
      })
      .on("error", e => {
        console.log(`sock err: ${e}`);
      });
  }

  static onConnect(req, clientSocket, head) {
    const net = require("net");
    // Connect to an origin server
    const { port, hostname } = new URL(`https://${req.url}`);
    const serverSocket = net
      .connect(port || 443, hostname, () => {
        clientSocket.write(
          "HTTP/1.1 200 Connection Established\r\n" +
            "Proxy-agent: Node.js-Proxy\r\n" +
            "\r\n"
        );
        serverSocket.write(head);
        serverSocket.pipe(clientSocket);
        clientSocket.pipe(serverSocket);
      })
      .on("error", e => {
        // The socket will error out when we kill the connection
        // just ignore it.
      });
  }
}

class BaseHTTPProxy extends BaseNodeServer {
  registerFilter() {
    const pps = Cc[
      "@mozilla.org/network/protocol-proxy-service;1"
    ].getService();
    this.filter = new NodeProxyFilter(
      this.protocol(),
      "localhost",
      this.port(),
      0
    );
    pps.registerFilter(this.filter, 10);
    registerCleanupFunction(() => {
      this.unregisterFilter();
    });
  }

  unregisterFilter() {
    const pps = Cc[
      "@mozilla.org/network/protocol-proxy-service;1"
    ].getService();
    if (this.filter) {
      pps.unregisterFilter(this.filter);
      this.filter = undefined;
    }
  }

  /// Stops the server
  async stop() {
    this.unregisterFilter();
    await super.stop();
  }
}

// HTTP1 Proxy

class NodeProxyFilter {
  constructor(type, host, port, flags) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this.QueryInterface = ChromeUtils.generateQI(["nsIProtocolProxyFilter"]);
  }
  applyFilter(uri, pi, cb) {
    if (
      uri.pathQueryRef.startsWith("/execute") ||
      uri.pathQueryRef.startsWith("/fork") ||
      uri.pathQueryRef.startsWith("/kill")
    ) {
      // So we allow NodeServer.execute to work
      cb.onProxyFilterResult(pi);
      return;
    }
    const pps = Cc[
      "@mozilla.org/network/protocol-proxy-service;1"
    ].getService();
    cb.onProxyFilterResult(
      pps.newProxyInfo(
        this._type,
        this._host,
        this._port,
        "",
        "",
        this._flags,
        1000,
        null
      )
    );
  }
}

class HTTPProxyCode {
  static async startServer(port) {
    const http = require("http");
    global.proxy = http.createServer(BaseProxyCode.proxyHandler);
    global.proxy.on("connect", BaseProxyCode.onConnect);

    await global.proxy.listen(port);
    let proxyPort = global.proxy.address().port;
    await ADB.forwardPort(proxyPort);
    return proxyPort;
  }
}

class NodeHTTPProxyServer extends BaseHTTPProxy {
  _protocol = "http";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseProxyCode);
    await this.execute(HTTPProxyCode);
    await this.execute(ADB);
    this._port = await this.execute(`HTTPProxyCode.startServer(${port})`);

    this.registerFilter();
  }
}

// HTTPS proxy

class HTTPSProxyCode {
  static async startServer(port) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };
    const https = require("https");
    global.proxy = https.createServer(options, BaseProxyCode.proxyHandler);
    global.proxy.on("connect", BaseProxyCode.onConnect);

    await global.proxy.listen(port);
    let proxyPort = global.proxy.address().port;
    await ADB.forwardPort(proxyPort);
    return proxyPort;
  }
}

class NodeHTTPSProxyServer extends BaseHTTPProxy {
  _protocol = "https";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseProxyCode);
    await this.execute(HTTPSProxyCode);
    await this.execute(ADB);
    this._port = await this.execute(`HTTPSProxyCode.startServer(${port})`);

    this.registerFilter();
  }
}

// HTTP2 proxy

class HTTP2ProxyCode {
  static async startServer(port) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };
    const https = require("https");
    global.proxy = https.createServer(options, BaseProxyCode.proxyHandler);
    global.proxy.on("connect", BaseProxyCode.onConnect);

    await global.proxy.listen(port);
    let proxyPort = global.proxy.address().port;
    await ADB.forwardPort(proxyPort);
    return proxyPort;
  }
}

class NodeHTTP2ProxyServer extends BaseHTTPProxy {
  _protocol = "https";
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseProxyCode);
    await this.execute(HTTP2ProxyCode);
    await this.execute(ADB);
    this._port = await this.execute(`HTTP2ProxyCode.startServer(${port})`);

    this.registerFilter();
  }
}

// Helper functions

async function with_node_servers(arrayOfClasses, asyncClosure) {
  for (let s of arrayOfClasses) {
    let server = new s();
    await server.start();
    registerCleanupFunction(async () => {
      await server.stop();
    });
    await asyncClosure(server);
    await server.stop();
  }
}
