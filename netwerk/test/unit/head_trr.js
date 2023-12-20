/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

/* globals require, __dirname, global, Buffer, process */

/// Sets the TRR related prefs and adds the certificate we use for the HTTP2
/// server.
function trr_test_setup() {
  dump("start!\n");

  let h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setBoolPref("network.http.http2.enabled", true);
  // the TRR server is on 127.0.0.1
  if (AppConstants.platform == "android") {
    Services.prefs.setCharPref("network.trr.bootstrapAddr", "10.0.2.2");
  } else {
    Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");
  }

  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  Services.prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  // some tests rely on the cache not being cleared on pref change.
  // we specifically test that this works
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  // Turn off strict fallback mode and TRR retry for most tests,
  // it is tested specifically.
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", false);

  // Turn off temp blocklist feature in tests. When enabled we may issue a
  // lookup to resolve a parent name when blocklisting, which may bleed into
  // and interfere with subsequent tasks.
  Services.prefs.setBoolPref("network.trr.temp_blocklist", false);

  // We intentionally don't set the TRR mode. Each test should set it
  // after setup in the first test.

  return h2Port;
}

/// Clears the prefs that we're likely to set while testing TRR code
function trr_clear_prefs() {
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.trr.uri");
  Services.prefs.clearUserPref("network.trr.credentials");
  Services.prefs.clearUserPref("network.trr.wait-for-portal");
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.confirmationNS");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");
  Services.prefs.clearUserPref("network.trr.temp_blocklist_duration_sec");
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.clearUserPref("network.trr.early-AAAA");
  Services.prefs.clearUserPref("network.trr.excluded-domains");
  Services.prefs.clearUserPref("network.trr.builtin-excluded-domains");
  Services.prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  Services.prefs.clearUserPref("network.trr.fetch_off_main_thread");
  Services.prefs.clearUserPref("captivedetect.canonicalURL");

  Services.prefs.clearUserPref("network.http.http2.enabled");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
  Services.prefs.clearUserPref(
    "network.trr.send_empty_accept-encoding_headers"
  );
  Services.prefs.clearUserPref("network.trr.strict_native_fallback");
  Services.prefs.clearUserPref("network.trr.temp_blocklist");
}

/// This class sends a DNS query and can be awaited as a promise to get the
/// response.
class TRRDNSListener {
  constructor(...args) {
    if (args.length < 2) {
      Assert.ok(false, "TRRDNSListener requires at least two arguments");
    }
    this.name = args[0];
    if (typeof args[1] == "object") {
      this.options = args[1];
    } else {
      this.options = {
        expectedAnswer: args[1],
        expectedSuccess: args[2] ?? true,
        delay: args[3],
        trrServer: args[4] ?? "",
        expectEarlyFail: args[5] ?? "",
        flags: args[6] ?? 0,
        type: args[7] ?? Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
        port: args[8] ?? -1,
      };
    }
    this.expectedAnswer = this.options.expectedAnswer ?? undefined;
    this.expectedSuccess = this.options.expectedSuccess ?? true;
    this.delay = this.options.delay;
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
    this.type = this.options.type ?? Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT;
    let trrServer = this.options.trrServer || "";
    let port = this.options.port || -1;

    // This may be called in a child process that doesn't have Services available.
    // eslint-disable-next-line mozilla/use-services
    const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
      Ci.nsIThreadManager
    );
    const currentThread = threadManager.currentThread;

    this.additionalInfo =
      trrServer == "" && port == -1
        ? null
        : Services.dns.newAdditionalInfo(trrServer, port);
    try {
      this.request = Services.dns.asyncResolve(
        this.name,
        this.type,
        this.options.flags || 0,
        this.additionalInfo,
        this,
        currentThread,
        this.options.originAttributes || {} // defaultOriginAttributes
      );
      Assert.ok(!this.options.expectEarlyFail, "asyncResolve ok");
    } catch (e) {
      Assert.ok(this.options.expectEarlyFail, "asyncResolve fail");
      this.resolve({ error: e });
    }
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.ok(
      inRequest == this.request,
      "Checking that this is the correct callback"
    );

    // If we don't expect success here, just resolve and the caller will
    // decide what to do with the results.
    if (!this.expectedSuccess) {
      this.resolve({ inRequest, inRecord, inStatus });
      return;
    }

    Assert.equal(inStatus, Cr.NS_OK, "Checking status");

    if (this.type != Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT) {
      this.resolve({ inRequest, inRecord, inStatus });
      return;
    }

    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    let answer = inRecord.getNextAddrAsString();
    Assert.equal(
      answer,
      this.expectedAnswer,
      `Checking result for ${this.name}`
    );
    inRecord.rewind(); // In case the caller also checks the addresses

    if (this.delay !== undefined) {
      Assert.greaterOrEqual(
        inRecord.trrFetchDurationNetworkOnly,
        this.delay,
        `the response should take at least ${this.delay}`
      );

      Assert.greaterOrEqual(
        inRecord.trrFetchDuration,
        this.delay,
        `the response should take at least ${this.delay}`
      );

      if (this.delay == 0) {
        // The response timing should be really 0
        Assert.equal(
          inRecord.trrFetchDurationNetworkOnly,
          0,
          `the response time should be 0`
        );

        Assert.equal(
          inRecord.trrFetchDuration,
          this.delay,
          `the response time should be 0`
        );
      }
    }

    this.resolve({ inRequest, inRecord, inStatus });
  }

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) || aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }

  // Implement then so we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }

  cancel(aStatus = Cr.NS_ERROR_ABORT) {
    Services.dns.cancelAsyncResolve(
      this.name,
      this.type,
      this.options.flags || 0,
      this.resolverInfo,
      this,
      aStatus,
      {}
    );
  }
}

// This is for reteriiving the raw bytes from a DNS answer.
function answerHandler(req, resp) {
  let searchParams = new URL(req.url, "http://example.com").searchParams;
  console.log("req.searchParams:" + searchParams);
  if (!searchParams.get("host")) {
    resp.writeHead(400);
    resp.end("Missing search parameter");
    return;
  }

  function processRequest(req1, resp1) {
    let domain = searchParams.get("host");
    let type = searchParams.get("type");
    let response = global.dns_query_answers[`${domain}/${type}`] || {};
    let buf = global.dnsPacket.encode({
      type: "response",
      id: 0,
      flags: 0,
      questions: [],
      answers: response.answers || [],
      additionals: response.additionals || [],
    });
    let writeResponse = (resp2, buf2, context) => {
      try {
        let data = buf2.toString("hex");
        resp2.setHeader("Content-Length", data.length);
        resp2.writeHead(200, { "Content-Type": "plain/text" });
        resp2.write(data);
        resp2.end("");
      } catch (e) {}
    };

    writeResponse(resp1, buf, response);
  }

  processRequest(req, resp);
}

/// This is the default handler for /dns-query
/// It implements basic functionality for parsing the DoH packet, then
/// queries global.dns_query_answers for available answers for the DNS query.
function trrQueryHandler(req, resp, url) {
  let requestBody = Buffer.from("");
  let method = req.headers[global.http2.constants.HTTP2_HEADER_METHOD];
  let contentLength = req.headers["content-length"];

  if (method == "POST") {
    req.on("data", chunk => {
      requestBody = Buffer.concat([requestBody, chunk]);
      if (requestBody.length == contentLength) {
        processRequest(req, resp, requestBody);
      }
    });
  } else if (method == "GET") {
    if (!url.query.dns) {
      resp.writeHead(400);
      resp.end("Missing dns parameter");
      return;
    }

    requestBody = Buffer.from(url.query.dns, "base64");
    processRequest(req, resp, requestBody);
  } else {
    // unexpected method.
    resp.writeHead(405);
    resp.end("Unexpected method");
  }

  function processRequest(req1, resp1, payload) {
    let dnsQuery = global.dnsPacket.decode(payload);
    let domain = dnsQuery.questions[0].name;
    let type = dnsQuery.questions[0].type;
    let response = global.dns_query_answers[`${domain}/${type}`] || {};

    if (!global.dns_query_counts[domain]) {
      global.dns_query_counts[domain] = {};
    }
    global.dns_query_counts[domain][type] =
      global.dns_query_counts[domain][type] + 1 || 1;

    let flags = global.dnsPacket.RECURSION_DESIRED;
    if (!response.answers && !response.flags) {
      flags |= 2; // SERVFAIL
    }
    flags |= response.flags || 0;
    let buf = global.dnsPacket.encode({
      type: "response",
      id: dnsQuery.id,
      flags,
      questions: dnsQuery.questions,
      answers: response.answers || [],
      additionals: response.additionals || [],
    });

    let writeResponse = (resp2, buf2, context) => {
      try {
        if (context.error) {
          // If the error is a valid HTTP response number just write it out.
          if (context.error < 600) {
            resp2.writeHead(context.error);
            resp2.end("Intentional error");
            return;
          }

          // Bigger error means force close the session
          req1.stream.session.close();
          return;
        }
        resp2.setHeader("Content-Length", buf2.length);
        resp2.writeHead(200, { "Content-Type": "application/dns-message" });
        resp2.write(buf2);
        resp2.end("");
      } catch (e) {}
    };

    if (response.delay) {
      // This function is handled within the httpserver where setTimeout is
      // available.
      // eslint-disable-next-line no-undef
      setTimeout(
        arg => {
          writeResponse(arg[0], arg[1], arg[2]);
        },
        response.delay,
        [resp1, buf, response]
      );
      return;
    }

    writeResponse(resp1, buf, response);
  }
}

function getRequestCount(domain, type) {
  if (!global.dns_query_counts[domain]) {
    return 0;
  }
  return global.dns_query_counts[domain][type] || 0;
}

// A convenient wrapper around NodeServer
class TRRServer extends NodeHTTP2Server {
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    await super.start(port);
    await this.execute(`( () => {
      // key: string "name/type"
      // value: array [answer1, answer2]
      global.dns_query_answers = {};

      // key: domain
      // value: a map containing {key: type, value: number of requests}
      global.dns_query_counts = {};

      global.dnsPacket = require(\`\${__dirname}/../dns-packet\`);
      global.ip = require(\`\${__dirname}/../node_ip\`);
      global.http2 = require("http2");
    })()`);
    await this.registerPathHandler("/dns-query", trrQueryHandler);
    await this.registerPathHandler("/dnsAnswer", answerHandler);
    await this.execute(getRequestCount);
  }

  /// @name : string - name we're providing answers for. eg: foo.example.com
  /// @type : string - the DNS query type. eg: "A", "AAAA", "CNAME", etc
  /// @response : a map containing the response
  ///   answers: array of answers (hashmap) that dnsPacket can parse
  ///    eg: [{
  ///          name: "bar.example.com",
  ///          ttl: 55,
  ///          type: "A",
  ///          flush: false,
  ///          data: "1.2.3.4",
  ///        }]
  ///   additionals - array of answers (hashmap) to be added to the additional section
  ///   delay: int - if not 0 the response will be sent with after `delay` ms.
  ///   flags: int - flags to be set on the answer
  ///   error: int - HTTP status. If truthy then the response will send this status
  async registerDoHAnswers(name, type, response = {}) {
    let text = `global.dns_query_answers["${name}/${type}"] = ${JSON.stringify(
      response
    )}`;
    return this.execute(text);
  }

  async requestCount(domain, type) {
    return this.execute(`getRequestCount("${domain}", "${type}")`);
  }
}

// Implements a basic HTTP2 proxy server
class TRRProxyCode {
  static async startServer(endServerPort) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };

    const http2 = require("http2");
    global.proxy = http2.createSecureServer(options);
    this.setupProxy();
    global.endServerPort = endServerPort;

    await global.proxy.listen(0);

    let serverPort = global.proxy.address().port;
    return serverPort;
  }

  static closeProxy() {
    global.proxy.closeSockets();
    return new Promise(resolve => {
      global.proxy.close(resolve);
    });
  }

  static proxyRequestCount() {
    return global.proxy_stream_count;
  }

  static setupProxy() {
    if (!global.proxy) {
      throw new Error("proxy is null");
    }

    global.proxy_stream_count = 0;

    // We need to track active connections so we can forcefully close keep-alive
    // connections when shutting down the proxy.
    global.proxy.socketIndex = 0;
    global.proxy.socketMap = {};
    global.proxy.on("connection", function (socket) {
      let index = global.proxy.socketIndex++;
      global.proxy.socketMap[index] = socket;
      socket.on("close", function () {
        delete global.proxy.socketMap[index];
      });
    });
    global.proxy.closeSockets = function () {
      for (let i in global.proxy.socketMap) {
        global.proxy.socketMap[i].destroy();
      }
    };

    global.proxy.on("stream", (stream, headers) => {
      if (headers[":method"] !== "CONNECT") {
        // Only accept CONNECT requests
        stream.respond({ ":status": 405 });
        stream.end();
        return;
      }
      global.proxy_stream_count++;
      const net = require("net");
      const socket = net.connect(global.endServerPort, "127.0.0.1", () => {
        try {
          stream.respond({ ":status": 200 });
          socket.pipe(stream);
          stream.pipe(socket);
        } catch (exception) {
          console.log(exception);
          stream.close();
        }
      });
      socket.on("error", error => {
        throw new Error(
          `Unxpected error when conneting the HTTP/2 server from the HTTP/2 proxy during CONNECT handling: '${error}'`
        );
      });
    });
  }
}

class TRRProxy {
  // Starts the proxy
  async start(port) {
    info("TRRProxy start!");
    this.processId = await NodeServer.fork();
    info("processid=" + this.processId);
    await this.execute(TRRProxyCode);
    this.port = await this.execute(`TRRProxyCode.startServer(${port})`);
    Assert.notEqual(this.port, null);
  }

  // Executes a command in the context of the node server
  async execute(command) {
    return NodeServer.execute(this.processId, command);
  }

  // Stops the server
  async stop() {
    if (this.processId) {
      await NodeServer.execute(this.processId, `TRRProxyCode.closeProxy()`);
      await NodeServer.kill(this.processId);
    }
  }

  async request_count() {
    let data = await NodeServer.execute(
      this.processId,
      `TRRProxyCode.proxyRequestCount()`
    );
    return parseInt(data);
  }
}
