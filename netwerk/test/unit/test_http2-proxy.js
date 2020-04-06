/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test checks following expectations when using HTTP/2 proxy:
 *
 * - when we request https access, we don't create different sessions for
 *   different origins, only new tunnels inside a single session
 * - when the isolation key (`proxy_isolation`) is changed, new single session
 *   is created for new requests to same origins as before
 * - error code returned from the tunnel (a proxy error - not end-server
 *   error!) doesn't kill the existing session
 * - check we are seeing expected nsresult error codes on channels
 *   (nsIChannel.status) corresponding to different proxy status code
 *   responses (502, 504, 407, ...)
 * - check we don't try to ask for credentials or otherwise authenticate to
 *   the proxy when 407 is returned and there is no Proxy-Authenticate
 *   response header sent
 */

const pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();
const { NodeServer } = ChromeUtils.import("resource://testing-common/httpd.js");

let proxy_port;
let server_port;
let filter;

// See moz-http2
const proxy_auth = "authorization-token";
let proxy_isolation;

class ProxyFilter {
  constructor(type, host, port, flags) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIProtocolProxyFilter]);
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
    cb.onProxyFilterResult(
      pps.newProxyInfo(
        this._type,
        this._host,
        this._port,
        proxy_auth,
        proxy_isolation,
        this._flags,
        1000,
        null
      )
    );
  }
}

class UnxpectedAuthPrompt2 {
  constructor(signal) {
    this.signal = signal;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIAuthPrompt2]);
  }
  asyncPromptAuth() {
    this.signal.triggered = true;
    throw Cr.ERROR_UNEXPECTED;
  }
}

class AuthRequestor {
  constructor(prompt) {
    this.prompt = prompt;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIInterfaceRequestor]);
  }
  getInterface(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      return this.prompt();
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    // Using TYPE_DOCUMENT for the authentication dialog test, it'd be blocked for other types
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });
}

function get_response(channel, flags = CL_ALLOW_UNKNOWN_CL) {
  return new Promise(resolve => {
    channel.asyncOpen(
      new ChannelListener(
        (request, data) => {
          request.QueryInterface(Ci.nsIHttpChannel);
          const status = request.status;
          const http_code = status ? undefined : request.responseStatus;
          request.QueryInterface(Ci.nsIProxiedChannel);
          const proxy_connect_response_code =
            request.httpProxyConnectResponseCode;
          resolve({ status, http_code, data, proxy_connect_response_code });
        },
        null,
        flags
      )
    );
  });
}

let initial_session_count = 0;

class http2ProxyCode {
  static listen(server, envport) {
    if (!server) {
      return Promise.resolve(0);
    }

    let portSelection = 0;
    if (envport !== undefined) {
      try {
        portSelection = parseInt(envport, 10);
      } catch (e) {
        portSelection = -1;
      }
    }
    return new Promise(resolve => {
      server.listen(portSelection, "0.0.0.0", 2000, () => {
        resolve(server.address().port);
      });
    });
  }

  static startNewProxy() {
    const fs = require("fs");
    const options = {
      key: fs.readFileSync(__dirname + "/http2-cert.key"),
      cert: fs.readFileSync(__dirname + "/http2-cert.pem"),
    };
    const http2 = require("http2");
    global.proxy = http2.createSecureServer(options);
    this.setupProxy();
    return http2ProxyCode.listen(proxy).then(port => {
      return { port, success: true };
    });
  }

  static closeProxy() {
    proxy.closeSockets();
    return new Promise(resolve => {
      proxy.close(resolve);
    });
  }

  static proxySessionCount() {
    if (!proxy) {
      return 0;
    }
    return proxy.proxy_session_count;
  }

  static setupProxy() {
    if (!proxy) {
      throw new Error("proxy is null");
    }
    proxy.proxy_session_count = 0;
    proxy.on("session", () => {
      ++proxy.proxy_session_count;
    });

    // We need to track active connections so we can forcefully close keep-alive
    // connections when shutting down the proxy.
    proxy.socketIndex = 0;
    proxy.socketMap = {};
    proxy.on("connection", function(socket) {
      let index = proxy.socketIndex++;
      proxy.socketMap[index] = socket;
      socket.on("close", function() {
        delete proxy.socketMap[index];
      });
    });
    proxy.closeSockets = function() {
      for (let i in proxy.socketMap) {
        proxy.socketMap[i].destroy();
      }
    };

    proxy.on("stream", (stream, headers) => {
      if (headers[":method"] !== "CONNECT") {
        // Only accept CONNECT requests
        stream.respond({ ":status": 405 });
        stream.end();
        return;
      }

      const target = headers[":authority"];

      const authorization_token = headers["proxy-authorization"];
      if (
        "authorization-token" != authorization_token ||
        target == "407.example.com:443"
      ) {
        stream.respond({ ":status": 407 });
        // Deliberately send no Proxy-Authenticate header
        stream.end();
        return;
      }
      if (target == "404.example.com:443") {
        // 404 Not Found, a response code that a proxy should return when the host can't be found
        stream.respond({ ":status": 404 });
        stream.end();
        return;
      }
      if (target == "429.example.com:443") {
        // 429 Too Many Requests, a response code that a proxy should return when receiving too many requests
        stream.respond({ ":status": 429 });
        stream.end();
        return;
      }
      if (target == "502.example.com:443") {
        // 502 Bad Gateway, a response code mostly resembling immediate connection error
        stream.respond({ ":status": 502 });
        stream.end();
        return;
      }
      if (target == "504.example.com:443") {
        // 504 Gateway Timeout, did not receive a timely response from an upstream server
        stream.respond({ ":status": 504 });
        stream.end();
        return;
      }

      const net = require("net");
      const socket = net.connect(serverPort, "127.0.0.1", () => {
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
        throw `Unxpected error when conneting the HTTP/2 server from the HTTP/2 proxy during CONNECT handling: '${error}'`;
      });
    });
  }
}

function proxy_session_counter() {
  return new Promise(async resolve => {
    let data = await NodeServer.execute(
      processId,
      `http2ProxyCode.proxySessionCount()`
    );
    resolve(parseInt(data) - initial_session_count);
  });
}
let processId;
add_task(async function setup() {
  // Set to allow the cert presented by our H2 server
  do_get_profile();

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  server_port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(server_port, null);
  processId = await NodeServer.fork();
  await NodeServer.execute(processId, `serverPort = ${server_port}`);
  await NodeServer.execute(processId, http2ProxyCode);
  let proxy = await NodeServer.execute(
    processId,
    `http2ProxyCode.startNewProxy()`
  );
  proxy_port = proxy.port;
  Assert.notEqual(proxy_port, null);

  Services.prefs.setStringPref(
    "services.settings.server",
    `data:text/html,test`
  );

  Services.prefs.setBoolPref("network.http.spdy.enabled", true);
  Services.prefs.setBoolPref("network.http.spdy.enabled.http2", true);

  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  filter = new ProxyFilter("https", "localhost", proxy_port, 0);
  pps.registerFilter(filter, 10);

  initial_session_count = await proxy_session_counter();
  info(`Initial proxy session count = ${initial_session_count}`);
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("services.settings.server");
  Services.prefs.clearUserPref("network.http.spdy.enabled");
  Services.prefs.clearUserPref("network.http.spdy.enabled.http2");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");

  pps.unregisterFilter(filter);

  await NodeServer.execute(processId, `http2ProxyCode.closeProxy()`);
  await NodeServer.kill(processId);
});

/**
 * Test series beginning.
 */

// Check we reach the h2 end server and keep only one session with the proxy for two different origin.
// Here we use the first isolation token.
add_task(async function proxy_success_one_session() {
  proxy_isolation = "TOKEN1";

  const foo = await get_response(
    make_channel(`https://foo.example.com/random-request-1`)
  );
  const alt1 = await get_response(
    make_channel(`https://alt1.example.com/random-request-2`)
  );

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.proxy_connect_response_code, 200);
  Assert.equal(foo.http_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.ok(foo.data.match("You Win!"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.proxy_connect_response_code, 200);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.ok(alt1.data.match("You Win!"));
  Assert.equal(
    await proxy_session_counter(),
    1,
    "Created just one session with the proxy"
  );
});

// The proxy responses with 407 instead of 200 Connected, make sure we get a proper error
// code from the channel and not try to ask for any credentials.
add_task(async function proxy_auth_failure() {
  const chan = make_channel(`https://407.example.com/`);
  const auth_prompt = { triggered: false };
  chan.notificationCallbacks = new AuthRequestor(
    () => new UnxpectedAuthPrompt2(auth_prompt)
  );
  const { status, http_code, proxy_connect_response_code } = await get_response(
    chan,
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_AUTHENTICATION_FAILED);
  Assert.equal(proxy_connect_response_code, 407);
  Assert.equal(http_code, undefined);
  Assert.equal(auth_prompt.triggered, false, "Auth prompt didn't trigger");
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by 407"
  );
});

// 502 Bad gateway code returned by the proxy, still one session only, proper different code
// from the channel.
add_task(async function proxy_bad_gateway_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://502.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(proxy_connect_response_code, 502);
  Assert.equal(http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by 502 after 407"
  );
});

// Second 502 Bad gateway code returned by the proxy, still one session only with the proxy.
add_task(async function proxy_bad_gateway_failure_two() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://502.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(proxy_connect_response_code, 502);
  Assert.equal(http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by second 502"
  );
});

// 504 Gateway timeout code returned by the proxy, still one session only, proper different code
// from the channel.
add_task(async function proxy_gateway_timeout_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://504.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_GATEWAY_TIMEOUT);
  Assert.equal(proxy_connect_response_code, 504);
  Assert.equal(http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by 504 after 502"
  );
});

// 404 Not Found means the proxy could not resolve the host.  As for other error responses
// we still expect this not to close the existing session.
add_task(async function proxy_host_not_found_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://404.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_UNKNOWN_HOST);
  Assert.equal(proxy_connect_response_code, 404);
  Assert.equal(http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by 404 after 504"
  );
});

add_task(async function proxy_too_many_requests_failure() {
  const { status, http_code, proxy_connect_response_code } = await get_response(
    make_channel(`https://429.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(status, Cr.NS_ERROR_PROXY_TOO_MANY_REQUESTS);
  Assert.equal(proxy_connect_response_code, 429);
  Assert.equal(http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created by 429 after 504"
  );
});

// Make sure that the above error codes don't kill the session and we still reach the end server
add_task(async function proxy_success_still_one_session() {
  const foo = await get_response(
    make_channel(`https://foo.example.com/random-request-1`)
  );
  const alt1 = await get_response(
    make_channel(`https://alt1.example.com/random-request-2`)
  );

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.http_code, 200);
  Assert.equal(foo.proxy_connect_response_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.proxy_connect_response_code, 200);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.equal(
    await proxy_session_counter(),
    1,
    "No new session created after proxy error codes"
  );
});

// Have a new isolation key, this means we are expected to create a new, and again one only,
// session with the proxy to reach the end server.
add_task(async function proxy_success_isolated_session() {
  Assert.notEqual(proxy_isolation, "TOKEN2");
  proxy_isolation = "TOKEN2";

  const foo = await get_response(
    make_channel(`https://foo.example.com/random-request-1`)
  );
  const alt1 = await get_response(
    make_channel(`https://alt1.example.com/random-request-2`)
  );
  const lh = await get_response(
    make_channel(`https://localhost/random-request-3`)
  );

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.proxy_connect_response_code, 200);
  Assert.equal(foo.http_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.ok(foo.data.match("You Win!"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.proxy_connect_response_code, 200);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.ok(alt1.data.match("You Win!"));
  Assert.equal(lh.status, Cr.NS_OK);
  Assert.equal(lh.proxy_connect_response_code, 200);
  Assert.equal(lh.http_code, 200);
  Assert.ok(lh.data.match("random-request-3"));
  Assert.ok(lh.data.match("You Win!"));
  Assert.equal(
    await proxy_session_counter(),
    2,
    "Just one new session seen after changing the isolation key"
  );
});

// Check that error codes are still handled the same way with new isolation, just in case.
add_task(async function proxy_bad_gateway_failure_isolated() {
  const failure1 = await get_response(
    make_channel(`https://502.example.com/`),
    CL_EXPECT_FAILURE
  );
  const failure2 = await get_response(
    make_channel(`https://502.example.com/`),
    CL_EXPECT_FAILURE
  );

  Assert.equal(failure1.status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(failure1.proxy_connect_response_code, 502);
  Assert.equal(failure1.http_code, undefined);
  Assert.equal(failure2.status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(failure2.proxy_connect_response_code, 502);
  Assert.equal(failure2.http_code, undefined);
  Assert.equal(
    await proxy_session_counter(),
    2,
    "No new session created by 502"
  );
});
