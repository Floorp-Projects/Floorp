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

let proxy_port;
let server_port;

// See moz-http2
const proxy_auth = 'authorization-token';
let proxy_isolation;

class ProxyFilter {
  constructor(type, host, port, flags) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIProtocolProxyFilter]);
  }
  applyFilter(pps, uri, pi, cb) {
    if (uri.spec.match(/(\/proxy-session-counter)/)) {
      cb.onProxyFilterResult(pi);
      return;
    }
    cb.onProxyFilterResult(pps.newProxyInfo(
      this._type, this._host, this._port,
      proxy_auth, proxy_isolation, this._flags, 1000, null));
  }
};

class UnxpectedAuthPrompt2 {
  constructor(signal) {
    this.signal = signal;
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIAuthPrompt2]);
  }
  asyncPromptAuth() {
    this.signal.triggered = true;
    throw Cr.ERROR_UNEXPECTED;
  }
};

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
};

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
    channel.asyncOpen(new ChannelListener((request, data) => {
      request.QueryInterface(Ci.nsIHttpChannel);
      const status = request.status;
      const http_code = status ? undefined : request.responseStatus;

      resolve({ status, http_code, data });
    }, null, flags));
  });
}

let initial_session_count = 0;

function proxy_session_counter() {
  return new Promise(async resolve => {
    const channel = make_channel(`https://localhost:${server_port}/proxy-session-counter`);
    const { data } = await get_response(channel);
    resolve(parseInt(data) - initial_session_count);
  });
}

add_task(async function setup() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  server_port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(server_port, null);
  proxy_port = env.get("MOZHTTP2_PROXY_PORT");
  Assert.notEqual(proxy_port, null);

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setBoolPref("network.http.spdy.enabled", true);
  Services.prefs.setBoolPref("network.http.spdy.enabled.http2", true);

  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
    .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  pps.registerFilter(new ProxyFilter("https", "localhost", proxy_port, 0), 10);

  initial_session_count = await proxy_session_counter();
  info(`Initial proxy session count = ${initial_session_count}`);
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.http.spdy.enabled");
  Services.prefs.clearUserPref("network.http.spdy.enabled.http2");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
});

/**
 * Test series beginning.
 */

// Check we reach the h2 end server and keep only one session with the proxy for two different origin.
// Here we use the first isolation token.
add_task(async function proxy_success_one_session() {
  proxy_isolation = "TOKEN1";

  const foo = await get_response(make_channel(`https://foo.example.com/random-request-1`));
  const alt1 = await get_response(make_channel(`https://alt1.example.com/random-request-2`));

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.http_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.ok(foo.data.match("You Win!"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.ok(alt1.data.match("You Win!"));
  Assert.equal(await proxy_session_counter(), 1, "Created just one session with the proxy");
});

// The proxy responses with 407 instead of 200 Connected, make sure we get a proper error
// code from the channel and not try to ask for any credentials.
add_task(async function proxy_auth_failure() {
  const chan = make_channel(`https://407.example.com/`);
  const auth_prompt = { triggered: false };
  chan.notificationCallbacks = new AuthRequestor(() => new UnxpectedAuthPrompt2(auth_prompt));
  const { status, http_code } = await get_response(chan, CL_EXPECT_FAILURE);

  Assert.equal(status, Cr.NS_ERROR_PROXY_AUTHENTICATION_FAILED);
  Assert.equal(http_code, undefined);
  Assert.equal(auth_prompt.triggered, false, "Auth prompt didn't trigger");
  Assert.equal(await proxy_session_counter(), 1, "No new session created by 407");
});

// 502 Bad gateway code returned by the proxy, still one session only, proper different code
// from the channel.
add_task(async function proxy_bad_gateway_failure() {
  const { status, http_code } = await get_response(make_channel(`https://502.example.com/`), CL_EXPECT_FAILURE);

  Assert.equal(status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(http_code, undefined);
  Assert.equal(await proxy_session_counter(), 1, "No new session created by 502 after 407");
});

// Second 502 Bad gateway code returned by the proxy, still one session only with the proxy.
add_task(async function proxy_bad_gateway_failure_two() {
  const { status, http_code } = await get_response(make_channel(`https://502.example.com/`), CL_EXPECT_FAILURE);

  Assert.equal(status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(http_code, undefined);
  Assert.equal(await proxy_session_counter(), 1, "No new session created by second 502");
});

// 504 Gateway timeout code returned by the proxy, still one session only, proper different code
// from the channel.
add_task(async function proxy_gateway_timeout_failure() {
  const { status, http_code } = await get_response(make_channel(`https://504.example.com/`), CL_EXPECT_FAILURE);

  Assert.equal(status, Cr.NS_ERROR_PROXY_GATEWAY_TIMEOUT);
  Assert.equal(http_code, undefined);
  Assert.equal(await proxy_session_counter(), 1, "No new session created by 504 after 502");
});

// 404 Not Found means the proxy could not resolve the host.  As for other error responses
// we still expect this not to close the existing session.
add_task(async function proxy_host_not_found_failure() {
  const { status, http_code } = await get_response(make_channel(`https://404.example.com/`), CL_EXPECT_FAILURE);

  Assert.equal(status, Cr.NS_ERROR_UNKNOWN_HOST);
  Assert.equal(http_code, undefined);
  Assert.equal(await proxy_session_counter(), 1, "No new session created by 404 after 504");
});

// Make sure that the above error codes don't kill the session and we still reach the end server
add_task(async function proxy_success_still_one_session() {
  const foo = await get_response(make_channel(`https://foo.example.com/random-request-1`));
  const alt1 = await get_response(make_channel(`https://alt1.example.com/random-request-2`));

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.http_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.equal(await proxy_session_counter(), 1, "No new session created after proxy error codes");
});

// Have a new isolation key, this means we are expected to create a new, and again one only,
// session with the proxy to reach the end server.
add_task(async function proxy_success_isolated_session() {
  Assert.notEqual(proxy_isolation, "TOKEN2");
  proxy_isolation = "TOKEN2";

  const foo = await get_response(make_channel(`https://foo.example.com/random-request-1`));
  const alt1 = await get_response(make_channel(`https://alt1.example.com/random-request-2`));
  const lh = await get_response(make_channel(`https://localhost/random-request-3`));

  Assert.equal(foo.status, Cr.NS_OK);
  Assert.equal(foo.http_code, 200);
  Assert.ok(foo.data.match("random-request-1"));
  Assert.ok(foo.data.match("You Win!"));
  Assert.equal(alt1.status, Cr.NS_OK);
  Assert.equal(alt1.http_code, 200);
  Assert.ok(alt1.data.match("random-request-2"));
  Assert.ok(alt1.data.match("You Win!"));
  Assert.equal(lh.status, Cr.NS_OK);
  Assert.equal(lh.http_code, 200);
  Assert.ok(lh.data.match("random-request-3"));
  Assert.ok(lh.data.match("You Win!"));
  Assert.equal(await proxy_session_counter(), 2, "Just one new session seen after changing the isolation key");
});

// Check that error codes are still handled the same way with new isolation, just in case.
add_task(async function proxy_bad_gateway_failure_isolated() {
  const failure1 = await get_response(make_channel(`https://502.example.com/`), CL_EXPECT_FAILURE);
  const failure2 = await get_response(make_channel(`https://502.example.com/`), CL_EXPECT_FAILURE);

  Assert.equal(failure1.status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(failure1.http_code, undefined);
  Assert.equal(failure2.status, Cr.NS_ERROR_PROXY_BAD_GATEWAY);
  Assert.equal(failure2.http_code, undefined);
  Assert.equal(await proxy_session_counter(), 2, "No new session created by 502");
});
