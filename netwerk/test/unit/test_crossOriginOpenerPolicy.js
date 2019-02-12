"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");
const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

let testServer = null;

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

class OnStartListener {
  constructor(onStartCallback) {
    this.callback = onStartCallback;
    this.done = new Promise(resolve => { this.resolve = resolve; });
  }

  onStartRequest(request, context) {
    this.callback(request.QueryInterface(Ci.nsIHttpChannel));
  }

  onDataAvailable(request, context, stream, offset, count) {
    let string = NetUtil.readInputStreamToString(stream, count);
  }

  onStopRequest(request, context, status) { this.resolve(); }
}


function handler_null(metadata, response)
{
  info("request_null");
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  let body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function handler_same_origin(metadata, response)
{
  info("request_same_origin");
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
  let body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function handler_same_origin_allow_outgoing(metadata, response)
{
  info("request_same_origin");
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cross-Origin-Opener-Policy", "same-origin unsafe-allow-outgoing", false);
  let body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function handler_same_site(metadata, response)
{
  info("request_same_site");
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cross-Origin-Opener-Policy", "same-site", false);
  let body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

function handler_same_site_allow_outgoing(metadata, response)
{
  info("request_same_site");
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cross-Origin-Opener-Policy", "same-site unsafe-allow-outgoing", false);
  let body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

add_task(async function test_setup() {
  if (!inChildProcess()) {
    Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com, example.com, example.org");
    registerCleanupFunction(function() {
      Services.prefs.clearUserPref("network.dns.localDomains");
    });
  }

  testServer = new HttpServer();
  testServer.start(-1);
  registerCleanupFunction(() => testServer.stop(() => {}));
  testServer.registerPathHandler("/null", handler_null);
  testServer.registerPathHandler("/same-origin", handler_same_origin);
  testServer.registerPathHandler("/same-origin_allow-outgoing", handler_same_origin_allow_outgoing);
  testServer.registerPathHandler("/same-site", handler_same_site);
  testServer.registerPathHandler("/same-site_allow-outgoing", handler_same_site_allow_outgoing);
  testServer.identity.setPrimary("http", "example.com", testServer.identity.primaryPort);
});

// policyA, originA, policyB, originB
async function generate_test(policyA, originA, policyB, originB, expectedResult, contentPolicyType = Ci.nsIContentPolicy.TYPE_DOCUMENT) {
  let listener = new OnStartListener((channel) => {
    if (!inChildProcess()) {
      equal(channel.hasCrossOriginOpenerPolicyMismatch(), expectedResult, `check for mismatch testing ${policyA}, ${originA}, ${policyB}, ${originB}, ${expectedResult}`);
    }
  });

  let chan = NetUtil.newChannel({
    uri: `${originB}:${testServer.identity.primaryPort}/${policyB}`,
    loadUsingSystemPrincipal: true,
    contentPolicyType: contentPolicyType,
  }).QueryInterface(Ci.nsIHttpChannel);

  if (policyA == "null") {
    chan.loadInfo.openerPolicy = Ci.nsILoadInfo.OPENER_POLICY_NULL;
  } else if (policyA == "same-origin") {
    chan.loadInfo.openerPolicy = Ci.nsILoadInfo.OPENER_POLICY_SAME_ORIGIN;
  } else if (policyA == "same-origin_allow-outgoing") {
    chan.loadInfo.openerPolicy = Ci.nsILoadInfo.OPENER_POLICY_SAME_ORIGIN_ALLOW_OUTGOING;
  } else if (policyA == "same-site") {
    chan.loadInfo.openerPolicy = Ci.nsILoadInfo.OPENER_POLICY_SAME_SITE;
  } else if (policyA == "same-site_allow-outgoing") {
    chan.loadInfo.openerPolicy = Ci.nsILoadInfo.OPENER_POLICY_SAME_SITE_ALLOW_OUTGOING;
  }

  let principalA = Services.scriptSecurityManager.createNullPrincipal({});
  if (originA != "about:blank") {
    principalA = Services.scriptSecurityManager.createCodebasePrincipal(Services.io.newURI(`${originA}:${testServer.identity.primaryPort}/${policyA}`), {});
  }

  chan.QueryInterface(Ci.nsIHttpChannelInternal).setTopWindowPrincipal(principalA);
  equal(chan.loadInfo.externalContentPolicyType, contentPolicyType);

  if (inChildProcess()) {
    do_send_remote_message("prepare-test", {"channelId": chan.channelId,
                                            "policyA": policyA,
                                            "originA": `${originA}`,
                                            "policyB": policyB,
                                            "originB": `${originB}`,
                                            "expectedResult": expectedResult});
    await do_await_remote_message("test-ready");
  }

  chan.asyncOpen2(listener);
  return listener.done;
}

add_task(async function test_policies() {
  // Note: This test only verifies that the result of
  // nsIHttpChannel.hasCrossOriginOpenerPolicyMismatch() is correct. It does not
  // check that the header is honored for auxiliary browsing contexts, how it
  // affects a top-level browsing context opened from a sandboxed iframe,
  // whether it affects the browsing context name, etc

  await generate_test("null", "http://example.com", "null", "http://example.com", false);
  await generate_test("null", "http://example.com", "same-origin", "http://example.com", true);
  await generate_test("null", "http://example.com", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("null", "http://example.com", "same-site", "http://example.com", true);
  await generate_test("null", "http://example.com", "same-site_allow-outgoing", "http://example.com", true);
  await generate_test("null", "about:blank", "null", "http://example.com", false);
  await generate_test("null", "about:blank", "same-origin", "http://example.com", true);
  await generate_test("null", "about:blank", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("null", "about:blank", "same-site", "http://example.com", true);
  await generate_test("null", "about:blank", "same-site_allow-outgoing", "http://example.com", true);

  await generate_test("same-origin", "http://example.com", "null", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.com", "null", "http://example.com", true);
  await generate_test("same-origin", "about:blank", "null", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "about:blank", "null", "http://example.com", false);
  await generate_test("same-origin", "http://example.com", "same-origin", "http://example.com", false);
  await generate_test("same-origin_allow-outgoing", "http://example.com", "same-origin", "http://example.com", true);
  await generate_test("same-origin", "http://example.com", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.com", "same-origin_allow-outgoing", "http://example.com", false);
  await generate_test("same-origin", "https://example.com", "same-origin", "http://example.com", true);
  // true because policyB is not null
  await generate_test("same-origin", "about:blank", "same-origin", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "about:blank", "same-origin", "http://example.com", true);
  await generate_test("same-origin", "about:blank", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "about:blank", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin", "http://foo.example.com", "same-origin", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://foo.example.com", "same-origin", "http://example.com", true);
  await generate_test("same-origin", "http://foo.example.com", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://foo.example.com", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin", "http://example.org", "same-origin", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.org", "same-origin", "http://example.com", true);
  await generate_test("same-origin", "http://example.org", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.org", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin", "http://example.com", "same-site", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.com", "same-site", "http://example.com", true);
  await generate_test("same-origin", "http://example.com", "same-site_allow-outgoing", "http://example.com", true);
  await generate_test("same-origin_allow-outgoing", "http://example.com", "same-site_allow-outgoing", "http://example.com", true);

  await generate_test("same-site", "http://example.com", "null", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "http://example.com", "null", "http://example.com", true);
  await generate_test("same-site", "about:blank", "null", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "about:blank", "null", "http://example.com", false);
  await generate_test("same-site", "http://example.com", "same-origin", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "http://example.com", "same-origin", "http://example.com", true);
  await generate_test("same-site", "http://example.com", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "http://example.com", "same-origin_allow-outgoing", "http://example.com", true);
  // true because policyB is not null
  await generate_test("same-site", "about:blank", "same-origin", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "about:blank", "same-origin", "http://example.com", true);
  await generate_test("same-site", "about:blank", "same-origin_allow-outgoing", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "about:blank", "same-origin_allow-outgoing", "http://example.com", true);
  // true because they have different schemes
  await generate_test("same-site", "https://example.com", "same-site", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "https://example.com", "same-site", "http://example.com", true);
  await generate_test("same-site", "https://example.com", "same-site_allow-outgoing", "http://example.com", true);
  await generate_test("same-site_allow-outgoing", "https://example.com", "same-site_allow-outgoing", "http://example.com", true);

  // These return false because the contentPolicyType is not TYPE_DOCUMENT
  await generate_test("null", "http://example.com", "same-origin", "http://example.com", false, Ci.nsIContentPolicy.TYPE_OTHER);
  await generate_test("same-origin", "http://example.com", "null", "http://example.com", false, Ci.nsIContentPolicy.TYPE_OTHER);

  if (inChildProcess()) {
    // send one message with no payload to clear the listener
    info("finishing");
    do_send_remote_message("prepare-test");
    await do_await_remote_message("test-ready");
  }
});
