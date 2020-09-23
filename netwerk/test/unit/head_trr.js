/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */

/* globals require, __dirname, global, Buffer */

const { NodeServer } = ChromeUtils.import("resource://testing-common/httpd.js");

/// Sets the TRR related prefs and adds the certificate we use for the HTTP2
/// server.
function trr_test_setup() {
  dump("start!\n");

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setBoolPref("network.http.spdy.enabled", true);
  Services.prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  Services.prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // use the h2 server as DOH provider
  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - reserved, 2 - TRR first, 3  - TRR only, 4 - reserved
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR first
  Services.prefs.setBoolPref("network.trr.wait-for-portal", false);
  // By default wait for all responses before notifying the listeners.
  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", true);
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
  Services.prefs.clearUserPref("network.trr.bootstrapAddress");
  Services.prefs.clearUserPref("network.trr.blacklist-duration");
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.clearUserPref("network.trr.early-AAAA");
  Services.prefs.clearUserPref("network.trr.skip-AAAA-when-not-supported");
  Services.prefs.clearUserPref("network.trr.wait-for-A-and-AAAA");
  Services.prefs.clearUserPref("network.trr.excluded-domains");
  Services.prefs.clearUserPref("network.trr.builtin-excluded-domains");
  Services.prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  Services.prefs.clearUserPref("network.trr.fetch_off_main_thread");
  Services.prefs.clearUserPref("captivedetect.canonicalURL");

  Services.prefs.clearUserPref("network.http.spdy.enabled");
  Services.prefs.clearUserPref("network.http.spdy.enabled.http2");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
  Services.prefs.clearUserPref(
    "network.trr.send_empty_accept-encoding_headers"
  );
}

/// This class sends a DNS query and can be awaited as a promise to get the
/// response.
class TRRDNSListener {
  constructor(name, options = {}) {
    this.name = name;
    this.options = options;
    this.expectedAnswer = options.expectedAnswer ?? undefined;
    this.expectedSuccess = options.expectedSuccess ?? true;
    this.delay = options.delay;
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
    let trrServer = options.trrServer || "";

    const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
      Ci.nsIDNSService
    );
    const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
      Ci.nsIThreadManager
    );
    const currentThread = threadManager.currentThread;

    let resolverInfo =
      trrServer == "" ? null : dns.newTRRResolverInfo(trrServer);
    try {
      this.request = dns.asyncResolve(
        name,
        Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
        this.options.flags || 0,
        resolverInfo,
        this,
        currentThread,
        {} // defaultOriginAttributes
      );
      Assert.ok(!options.expectEarlyFail);
    } catch (e) {
      Assert.ok(options.expectEarlyFail);
      this.resolve([e]);
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
      this.resolve([inRequest, inRecord, inStatus]);
      return;
    }

    Assert.equal(inStatus, Cr.NS_OK, "Checking status");
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

    this.resolve([inRequest, inRecord, inStatus]);
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
}

/// Implements a basic HTTP2 server
class TRRServerCode {
  static async startServer(port) {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };

    const url = require("url");
    global.path_handlers = {};
    global.handler = (req, resp) => {
      const path = req.headers[global.http2.constants.HTTP2_HEADER_PATH];
      let u = url.parse(req.url, true);
      let handler = global.path_handlers[u.pathname];
      if (handler) {
        return handler(req, resp, u);
      }

      // Didn't find a handler for this path.
      let response = `<h1> 404 Path not found: ${path}</h1>`;
      resp.setHeader("Content-Type", "text/html");
      resp.setHeader("Content-Length", response.length);
      resp.writeHead(404);
      resp.end(response);
    };

    // key: string "name/type"
    // value: array [answer1, answer2]
    global.dns_query_answers = {};

    global.http2 = require("http2");
    global.server = global.http2.createSecureServer(options, global.handler);

    await global.server.listen(port);

    global.dnsPacket = require(`${__dirname}/../dns-packet`);
    global.ip = require(`${__dirname}/../node-ip`);

    return global.server.address().port;
  }
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
        return processRequest(req, resp, requestBody);
      }
    });
  } else if (method == "GET") {
    if (!url.query.dns) {
      resp.writeHead(400);
      resp.end("Missing dns parameter");
      return;
    }

    requestBody = Buffer.from(url.query.dns, "base64");
    return processRequest(req, resp, requestBody);
  } else {
    // unexpected method.
    resp.writeHead(405);
    resp.end("Unexpected method");
  }

  function processRequest(req, resp, payload) {
    let dnsQuery = global.dnsPacket.decode(payload);
    let response =
      global.dns_query_answers[
        `${dnsQuery.questions[0].name}/${dnsQuery.questions[0].type}`
      ] || {};

    let buf = global.dnsPacket.encode({
      type: "response",
      id: dnsQuery.id,
      flags: global.dnsPacket.RECURSION_DESIRED,
      questions: dnsQuery.questions,
      answers: response.answers || [],
      additionals: response.additionals || [],
    });

    let writeResponse = (resp, buf) => {
      resp.setHeader("Content-Length", buf.length);
      resp.writeHead(200, { "Content-Type": "application/dns-message" });
      resp.write(buf);
      resp.end("");
    };

    if (response.delay) {
      setTimeout(
        arg => {
          writeResponse(arg[0], arg[1]);
        },
        response.delay,
        [resp, buf]
      );
      return;
    }

    writeResponse(resp, buf);
  }
}

// A convenient wrapper around NodeServer
class TRRServer {
  /// Starts the server
  /// @port - default 0
  ///    when provided, will attempt to listen on that port.
  async start(port = 0) {
    this.processId = await NodeServer.fork();

    await this.execute(TRRServerCode);
    this.port = await this.execute(`TRRServerCode.startServer(${port})`);
    await this.registerPathHandler("/dns-query", trrQueryHandler);
  }

  /// Executes a command in the context of the node server
  async execute(command) {
    return NodeServer.execute(this.processId, command);
  }

  /// Stops the server
  async stop() {
    if (this.processId) {
      await NodeServer.kill(this.processId);
      this.processId = undefined;
    }
  }

  /// @path : string - the path on the server that we're handling. ex: /path
  /// @handler : function(req, resp, url) - function that processes request and
  ///     emits a response.
  async registerPathHandler(path, handler) {
    return this.execute(
      `global.path_handlers["${path}"] = ${handler.toString()}`
    );
  }

  /// @name : string - name we're providing answers for. eg: foo.example.com
  /// @type : string - the DNS query type. eg: "A", "AAAA", "CNAME", etc
  /// @answers : array - array of answers (hashmap) that dnsPacket can parse
  ///    eg: [{
  ///          name: "bar.example.com",
  ///          ttl: 55,
  ///          type: "A",
  ///          flush: false,
  ///          data: "1.2.3.4",
  ///        }]
  async registerDoHAnswers(name, type, answers, additionals, delay = 0) {
    let text = `global.dns_query_answers["${name}/${type}"] = ${JSON.stringify({
      answers,
      additionals,
      delay,
    })}`;
    return this.execute(text);
  }
}
