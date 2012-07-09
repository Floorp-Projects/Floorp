/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "IDService",
                                  "resource://gre/modules/identity/Identity.jsm",
                                  "IdentityService");

const WELL_KNOWN_PATH = "/.well-known/browserid";

let SERVER_PORT = 8080;

// valid IDP
function test_well_known_1() {
  do_test_pending();

  let server = new nsHttpServer();
  server.registerFile(WELL_KNOWN_PATH, do_get_file("data/idp_1" + WELL_KNOWN_PATH));
  server.start(SERVER_PORT);
  let hostPort = "localhost:" + SERVER_PORT;

  function check_well_known(aErr, aCallbackObj) {
    do_check_null(aErr);
    do_check_eq(aCallbackObj.domain, hostPort);
    let idpParams = aCallbackObj.idpParams;
    do_check_eq(idpParams['public-key'].algorithm, "RS");
    do_check_eq(idpParams.authentication, "/browserid/sign_in.html");
    do_check_eq(idpParams.provisioning, "/browserid/provision.html");

    do_test_finished();
    server.stop(run_next_test);
  }

  IDService._fetchWellKnownFile(hostPort, check_well_known, "http");
}

// valid domain, non-exixtent browserid file
function test_well_known_404() {
  do_test_pending();

  let server = new nsHttpServer();
  // Don't register the well-known file
  // Change ports to avoid HTTP caching
  SERVER_PORT++;
  server.start(SERVER_PORT);

  let hostPort = "localhost:" + SERVER_PORT;

  function check_well_known_404(aErr, aCallbackObj) {
    do_check_eq("Error", aErr);
    do_check_eq(undefined, aCallbackObj);
    do_test_finished();
    server.stop(run_next_test);
  }

  IDService._fetchWellKnownFile(hostPort, check_well_known_404, "http");
}

// valid domain, invalid browserid file (no "provisioning" member)
function test_well_known_invalid_1() {
  do_test_pending();

  let server = new nsHttpServer();
  server.registerFile(WELL_KNOWN_PATH, do_get_file("data/idp_invalid_1" + WELL_KNOWN_PATH));
  // Change ports to avoid HTTP caching
  SERVER_PORT++;
  server.start(SERVER_PORT);

  let hostPort = "localhost:" + SERVER_PORT;

  function check_well_known_invalid_1(aErr, aCallbackObj) {
    // check for an error message
    do_check_true(aErr && aErr.length > 0);
    do_check_eq(undefined, aCallbackObj);
    do_test_finished();
    server.stop(run_next_test);
  }

  IDService._fetchWellKnownFile(hostPort, check_well_known_invalid_1, "http");
}

let TESTS = [test_well_known_1, test_well_known_404, test_well_known_invalid_1];

TESTS.forEach(add_test);

function run_test() {
  run_next_test();
}
