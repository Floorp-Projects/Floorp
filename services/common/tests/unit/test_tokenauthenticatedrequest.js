/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_authenticated_request() {
  _("Ensure that sending a MAC authenticated GET request works as expected.");

  let message = "Great Success!";

  // TODO: We use a preset key here, but use getTokenFromBrowserIDAssertion()
  // from TokenServerClient to get a real one when possible. (Bug 745800)
  let id = "eyJleHBpcmVzIjogMTM2NTAxMDg5OC4x";
  let key = "qTZf4ZFpAMpMoeSsX3zVRjiqmNs=";
  let method = "GET";

  let nonce = btoa(CryptoUtils.generateRandomBytes(16));
  let ts = Math.floor(Date.now() / 1000);
  let extra = {ts: ts, nonce: nonce};

  let auth;

  let server = httpd_setup({"/foo": function(request, response) {
      do_check_true(request.hasHeader("Authorization"));
      do_check_eq(auth, request.getHeader("Authorization"));

      response.setStatusLine(request.httpVersion, 200, "OK");
      response.bodyOutputStream.write(message, message.length);
    }
  });
  let uri = CommonUtils.makeURI(server.baseURI + "/foo");
  let sig = CryptoUtils.computeHTTPMACSHA1(id, key, method, uri, extra);
  auth = sig.getHeader();

  let req = new TokenAuthenticatedRESTRequest(uri, {id: id, key: key}, extra);
  let cb = Async.makeSpinningCallback();
  req.get(cb);
  let result = cb.wait();

  do_check_eq(null, result);
  do_check_eq(message, req.response.body);

  server.stop(run_next_test);
});
