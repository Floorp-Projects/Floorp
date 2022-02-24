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

class BaseNodeServer {
  port() {
    return this._port;
  }

  /// Stops the server
  async stop() {
    if (this.processId) {
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
    return global.server.address().port;
  }
}

class NodeHTTPServer extends BaseNodeServer {
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTPServerCode);
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
    return global.server.address().port;
  }
}

class NodeHTTPSServer extends BaseNodeServer {
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTPSServerCode);
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
    return global.server.address().port;
  }
}

class NodeHTTP2Server extends BaseNodeServer {
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(BaseNodeHTTPServerCode);
    await this.execute(NodeHTTP2ServerCode);
    this._port = await this.execute(`NodeHTTP2ServerCode.startServer(${port})`);
    await this.execute(`global.path_handlers = {};`);
  }
}
