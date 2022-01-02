/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");

const BinaryOutputStream = Components.Constructor(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);

const server = new HttpServer();
server.start(-1);
registerCleanupFunction(() => server.stop(() => {}));
const SERVER_BASE_URL = `http://localhost:${server.identity.primaryPort}`;

const proxyServer = new HttpServer();
proxyServer.identity.add("http", "localhost", server.identity.primaryPort);
proxyServer.start(-1);
registerCleanupFunction(() => proxyServer.stop(() => {}));
const PROXY_PORT = proxyServer.identity.primaryPort;

// A sequence of bytes that would become garbage if it were to be read as UTF-8:
// - 0xEF 0xBB 0xBF is a byte order mark.
// - 0xC0 on its own is invalid (it's the first byte of a 2-byte encoding).
const INVALID_UTF_8_BYTES = [0xef, 0xbb, 0xbf, 0xc0];

server.registerPathHandler("/binary.dat", (request, response) => {
  response.setStatusLine(null, 201, "StatusLineHere");
  response.setHeader("headerName", "HeaderValue: HeaderValueEnd");
  let binaryOut = new BinaryOutputStream(response.bodyOutputStream);
  binaryOut.writeByteArray([0xef, 0xbb, 0xbf, 0xc0]);
});

// HTTPS requests are proxied with CONNECT, but our test server is HTTP,
// which means that the proxy will receive GET http://localhost:port.
var proxiedCount = 0;
proxyServer.registerPrefixHandler("/", (request, response) => {
  ++proxiedCount;
  Assert.equal(request.path, "/binary.dat", `Proxy request ${proxiedCount}`);
  // Close connection without sending any response.
  response.seizePower();
  response.finish();
});

add_task(async function test_utils_fetch_binary() {
  let res = await Utils.fetch(`${SERVER_BASE_URL}/binary.dat`);

  Assert.equal(res.status, 201, "res.status");
  Assert.equal(res.statusText, "StatusLineHere", "res.statusText");
  Assert.equal(
    res.headers.get("headerName"),
    "HeaderValue: HeaderValueEnd",
    "Utils.fetch should return the header"
  );

  Assert.deepEqual(
    Array.from(new Uint8Array(await res.arrayBuffer())),
    INVALID_UTF_8_BYTES,
    "Binary response body should be returned as is"
  );
});

add_task(async function test_utils_fetch_binary_as_text() {
  let res = await Utils.fetch(`${SERVER_BASE_URL}/binary.dat`);
  Assert.deepEqual(
    Array.from(await res.text(), c => c.charCodeAt(0)),
    [65533],
    "Interpreted as UTF-8, the response becomes garbage"
  );
});

add_task(async function test_utils_fetch_binary_as_json() {
  let res = await Utils.fetch(`${SERVER_BASE_URL}/binary.dat`);
  await Assert.rejects(
    res.json(),
    /SyntaxError: JSON.parse: unexpected character/,
    "Binary data is invalid JSON"
  );
});

add_task(async function test_utils_fetch_has_conservative() {
  let channelPromise = TestUtils.topicObserved("http-on-modify-request");
  await Utils.fetch(`${SERVER_BASE_URL}/binary.dat`);

  let channel = (await channelPromise)[0].QueryInterface(Ci.nsIHttpChannel);

  Assert.equal(channel.URI.spec, `${SERVER_BASE_URL}/binary.dat`, "URL OK");

  let internalChannel = channel.QueryInterface(Ci.nsIHttpChannelInternal);
  Assert.ok(internalChannel.beConservative, "beConservative flag is set");
});

add_task(async function test_utils_fetch_has_conservative() {
  let channelPromise = TestUtils.topicObserved("http-on-modify-request");
  await Utils.fetch(`${SERVER_BASE_URL}/binary.dat`);

  let channel = (await channelPromise)[0].QueryInterface(Ci.nsIHttpChannel);

  Assert.equal(channel.URI.spec, `${SERVER_BASE_URL}/binary.dat`, "URL OK");

  let internalChannel = channel.QueryInterface(Ci.nsIHttpChannelInternal);
  Assert.ok(internalChannel.beConservative, "beConservative flag is set");
});

add_task(async function test_utils_fetch_with_bad_proxy() {
  Services.prefs.setIntPref("network.proxy.type", 1);
  Services.prefs.setStringPref("network.proxy.http", "127.0.0.1");
  Services.prefs.setIntPref("network.proxy.http_port", PROXY_PORT);
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  // The URL that we're going to request.
  const DESTINATION_URL = `${SERVER_BASE_URL}/binary.dat`;

  Assert.equal(proxiedCount, 0, "Proxy not used yet");
  {
    info("Bad proxy, default prefs");
    let res = await Utils.fetch(DESTINATION_URL);
    Assert.equal(res.status, 201, "Bypassed bad proxy");
    // 10 instead of 1 because of reconnect attempts after a dropped request.
    Assert.equal(proxiedCount, 10, "Proxy was used by HttpChannel");
  }

  // Disables the failover logic from HttpChannel.
  Services.prefs.setBoolPref("network.proxy.failover_direct", false);
  proxiedCount = 0;
  {
    info("Bad proxy, disabled network.proxy.failover_direct");
    let res = await Utils.fetch(DESTINATION_URL);
    Assert.equal(res.status, 201, "Bypassed bad proxy");
    // 10 instead of 1 because of reconnect attempts after a dropped request.
    Assert.equal(proxiedCount, 10, "Proxy was used by ServiceRequest");
  }

  proxiedCount = 0;
  {
    info("Using internal option of Utils.fetch: bypassProxy=true");
    let res = await Utils.fetch(DESTINATION_URL, { bypassProxy: true });
    Assert.equal(res.status, 201, "Bypassed bad proxy");
    Assert.equal(proxiedCount, 0, "Not using proxy when bypassProxy=true");
  }

  // Disables the failover logic from ServiceRequest/Utils.fetch
  Services.prefs.setBoolPref("network.proxy.allow_bypass", false);
  proxiedCount = 0;

  info("Bad proxy, disabled network.proxy.allow_bypass");
  await Assert.rejects(
    Utils.fetch(DESTINATION_URL),
    /NetworkError/,
    "Bad proxy request should fail without failover"
  );
  // 10 instead of 1 because of reconnect attempts after a dropped request.
  Assert.equal(proxiedCount, 10, "Attempted to use proxy again");

  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.http");
  Services.prefs.clearUserPref("network.proxy.http_port");
  Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  Services.prefs.clearUserPref("network.proxy.failover_direct");
  Services.prefs.clearUserPref("network.proxy.allow_bypass");
});
