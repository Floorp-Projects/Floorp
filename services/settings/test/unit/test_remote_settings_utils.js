/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
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
