/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);
const { TokenAuthenticatedRESTRequest } = ChromeUtils.import(
  "resource://services-common/rest.js"
);

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_task(async function test_authenticated_request() {
  _("Ensure that sending a MAC authenticated GET request works as expected.");

  let message = "Great Success!";

  // TODO: We use a preset key here, but use getTokenFromBrowserIDAssertion()
  // from TokenServerClient to get a real one when possible. (Bug 745800)
  let id = "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x";
  let key = "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=";
  let method = "GET";

  let nonce = btoa(CryptoUtils.generateRandomBytesLegacy(16));
  let ts = Math.floor(Date.now() / 1000);
  let extra = { ts, nonce };

  let auth;

  let server = httpd_setup({
    "/foo": function(request, response) {
      Assert.ok(request.hasHeader("Authorization"));
      Assert.equal(auth, request.getHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    },
  });
  let uri = CommonUtils.makeURI(server.baseURI + "/foo");
  let sig = await CryptoUtils.computeHTTPMACSHA1(id, key, method, uri, extra);
  auth = sig.getHeader();

  let req = new TokenAuthenticatedRESTRequest(uri, { id, key }, extra);
  await req.get();

  Assert.equal(message, req.response.body);

  await promiseStopServer(server);
});
