/* import-globals-from http2_test_common.js */

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var concurrent_channels = [];
var httpserv = null;
var httpserv2 = null;

var loadGroup;
var serverPort;

function altsvcHttp1Server(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Alt-Svc", 'h2=":' + serverPort + '"', false);
  var body = "this is where a cool kid would write something neat.\n";
  response.bodyOutputStream.write(body, body.length);
}

function h1ServerWK(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);

  var body = '["http://foo.example.com:' + httpserv.identity.primaryPort + '"]';
  response.bodyOutputStream.write(body, body.length);
}

function altsvcHttp1Server2(metadata, response) {
  // this server should never be used thanks to an alt svc frame from the
  // h2 server.. but in case of some async lag in setting the alt svc route
  // up we have it.
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Connection", "close", false);
  var body = "hanging.\n";
  response.bodyOutputStream.write(body, body.length);
}

function h1ServerWK2(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Connection", "close", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Method", "GET", false);

  var body =
    '["http://foo.example.com:' + httpserv2.identity.primaryPort + '"]';
  response.bodyOutputStream.write(body, body.length);
}

add_setup(async function setup() {
  serverPort = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(serverPort, null);
  dump("using port " + serverPort + "\n");

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert. Some older tests in
  // this suite use localhost with a TOFU exception, but new ones should use
  // foo.example.com
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  Services.prefs.setBoolPref("network.http.http2.enabled", true);
  Services.prefs.setBoolPref("network.http.http2.allow-push", true);
  Services.prefs.setBoolPref("network.http.altsvc.enabled", true);
  Services.prefs.setBoolPref("network.http.altsvc.oe", true);
  Services.prefs.setCharPref(
    "network.dns.localDomains",
    "foo.example.com, bar.example.com"
  );
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(
    Ci.nsILoadGroup
  );

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/altsvc1", altsvcHttp1Server);
  httpserv.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK);
  httpserv.start(-1);
  httpserv.identity.setPrimary(
    "http",
    "foo.example.com",
    httpserv.identity.primaryPort
  );

  httpserv2 = new HttpServer();
  httpserv2.registerPathHandler("/altsvc2", altsvcHttp1Server2);
  httpserv2.registerPathHandler("/.well-known/http-opportunistic", h1ServerWK2);
  httpserv2.start(-1);
  httpserv2.identity.setPrimary(
    "http",
    "foo.example.com",
    httpserv2.identity.primaryPort
  );
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  Services.prefs.clearUserPref("network.http.http2.enabled");
  Services.prefs.clearUserPref("network.http.http2.allow-push");
  Services.prefs.clearUserPref("network.http.altsvc.enabled");
  Services.prefs.clearUserPref("network.http.altsvc.oe");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  await httpserv.stop();
  await httpserv2.stop();
});

// hack - the header test resets the multiplex object on the server,
// so make sure header is always run before the multiplex test.
//
// make sure post_big runs first to test race condition in restarting
// a stalled stream when a SETTINGS frame arrives
add_task(async function do_test_http2_post_big() {
  const { httpProxyConnectResponseCode } = await test_http2_post_big(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_basic() {
  const { httpProxyConnectResponseCode } = await test_http2_basic(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_concurrent() {
  const { httpProxyConnectResponseCode } = await test_http2_concurrent(
    concurrent_channels,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_concurrent_post() {
  const { httpProxyConnectResponseCode } = await test_http2_concurrent_post(
    concurrent_channels,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_basic_unblocked_dep() {
  const { httpProxyConnectResponseCode } = await test_http2_basic_unblocked_dep(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_nospdy() {
  const { httpProxyConnectResponseCode } = await test_http2_nospdy(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push1() {
  const { httpProxyConnectResponseCode } = await test_http2_push1(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push2() {
  const { httpProxyConnectResponseCode } = await test_http2_push2(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push3() {
  const { httpProxyConnectResponseCode } = await test_http2_push3(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push4() {
  const { httpProxyConnectResponseCode } = await test_http2_push4(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push5() {
  const { httpProxyConnectResponseCode } = await test_http2_push5(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push6() {
  const { httpProxyConnectResponseCode } = await test_http2_push6(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_altsvc() {
  const { httpProxyConnectResponseCode } = await test_http2_altsvc(
    httpserv.identity.primaryPort,
    httpserv2.identity.primaryPort,
    false
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_doubleheader() {
  const { httpProxyConnectResponseCode } = await test_http2_doubleheader(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_xhr() {
  await test_http2_xhr(serverPort);
});

add_task(async function do_test_http2_header() {
  const { httpProxyConnectResponseCode } = await test_http2_header(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_invalid_response_header_name_spaces() {
  const { httpProxyConnectResponseCode } =
    await test_http2_invalid_response_header(serverPort, "name_spaces");
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(
  async function do_test_http2_invalid_response_header_value_line_feed() {
    const { httpProxyConnectResponseCode } =
      await test_http2_invalid_response_header(serverPort, "value_line_feed");
    Assert.equal(httpProxyConnectResponseCode, -1);
  }
);

add_task(
  async function do_test_http2_invalid_response_header_value_carriage_return() {
    const { httpProxyConnectResponseCode } =
      await test_http2_invalid_response_header(
        serverPort,
        "value_carriage_return"
      );
    Assert.equal(httpProxyConnectResponseCode, -1);
  }
);

add_task(async function do_test_http2_invalid_response_header_value_null() {
  const { httpProxyConnectResponseCode } =
    await test_http2_invalid_response_header(serverPort, "value_null");
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_cookie_crumbling() {
  const { httpProxyConnectResponseCode } = await test_http2_cookie_crumbling(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_multiplex() {
  var values = await test_http2_multiplex(serverPort);
  Assert.equal(values[0].httpProxyConnectResponseCode, -1);
  Assert.equal(values[1].httpProxyConnectResponseCode, -1);
  Assert.notEqual(values[0].streamID, values[1].streamID);
});

add_task(async function do_test_http2_big() {
  const { httpProxyConnectResponseCode } = await test_http2_big(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_huge_suspended() {
  const { httpProxyConnectResponseCode } = await test_http2_huge_suspended(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_post() {
  const { httpProxyConnectResponseCode } = await test_http2_post(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_empty_post() {
  const { httpProxyConnectResponseCode } = await test_http2_empty_post(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_patch() {
  const { httpProxyConnectResponseCode } = await test_http2_patch(serverPort);
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_pushapi_1() {
  const { httpProxyConnectResponseCode } = await test_http2_pushapi_1(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_continuations() {
  const { httpProxyConnectResponseCode } = await test_http2_continuations(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_blocking_download() {
  const { httpProxyConnectResponseCode } = await test_http2_blocking_download(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_illegalhpacksoft() {
  const { httpProxyConnectResponseCode } = await test_http2_illegalhpacksoft(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_illegalhpackhard() {
  const { httpProxyConnectResponseCode } = await test_http2_illegalhpackhard(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_folded_header() {
  const { httpProxyConnectResponseCode } = await test_http2_folded_header(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_empty_data() {
  const { httpProxyConnectResponseCode } = await test_http2_empty_data(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_status_phrase() {
  const { httpProxyConnectResponseCode } = await test_http2_status_phrase(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_doublepush() {
  const { httpProxyConnectResponseCode } = await test_http2_doublepush(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_disk_cache_push() {
  const { httpProxyConnectResponseCode } = await test_http2_disk_cache_push(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_h11required_stream() {
  // Add new tests above here - best to add new tests before h1
  // streams get too involved
  // These next two must always come in this order
  const { httpProxyConnectResponseCode } = await test_http2_h11required_stream(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_h11required_session() {
  const { httpProxyConnectResponseCode } = await test_http2_h11required_session(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_retry_rst() {
  const { httpProxyConnectResponseCode } = await test_http2_retry_rst(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_wrongsuite_tls12() {
  const { httpProxyConnectResponseCode } = await test_http2_wrongsuite_tls12(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_wrongsuite_tls13() {
  const { httpProxyConnectResponseCode } = await test_http2_wrongsuite_tls13(
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_firstparty1() {
  const { httpProxyConnectResponseCode } = await test_http2_push_firstparty1(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_firstparty2() {
  const { httpProxyConnectResponseCode } = await test_http2_push_firstparty2(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_firstparty3() {
  const { httpProxyConnectResponseCode } = await test_http2_push_firstparty3(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_userContext1() {
  const { httpProxyConnectResponseCode } = await test_http2_push_userContext1(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_userContext2() {
  const { httpProxyConnectResponseCode } = await test_http2_push_userContext2(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});

add_task(async function do_test_http2_push_userContext3() {
  const { httpProxyConnectResponseCode } = await test_http2_push_userContext3(
    loadGroup,
    serverPort
  );
  Assert.equal(httpProxyConnectResponseCode, -1);
});
