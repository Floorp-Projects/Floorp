/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

var httpProtocolHandler = Cc["@mozilla.org/network/protocol;1?name=http"]
                          .getService(Ci.nsIHttpProtocolHandler);

var collections = {steam:  65.11328,
                   petrol: 82.488281,
                   diesel: 2.25488281};

function run_test() {
  Log.repository.getLogger("Sync.Service").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.StorageRequest").level = Log.Level.Trace;
  initTestLogging();

  ensureLegacyIdentityManager();
  setBasicCredentials("johndoe", "ilovejane");

  run_next_test();
}

add_test(function test_success() {
  let handler = httpd_handler(200, "OK", JSON.stringify(collections));
  let server = httpd_setup({"/1.1/johndoe/info/collections": handler});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";

  let request = Service.getStorageInfo("collections", function(error, info) {
    do_check_eq(error, null);
    do_check_true(Utils.deepEquals(info, collections));

    // Ensure that the request is sent off with the right bits.
    do_check_true(basic_auth_matches(handler.request,
                                     Service.identity.username,
                                     Service.identity.basicPassword));
    let expectedUA = Services.appinfo.name + "/" + Services.appinfo.version +
                     " (" + httpProtocolHandler.oscpu + ")" +
                     " FxSync/" + WEAVE_VERSION + "." +
                     Services.appinfo.appBuildID + ".desktop";
    do_check_eq(handler.request.getHeader("User-Agent"), expectedUA);

    server.stop(run_next_test);
  });
  do_check_true(request instanceof RESTRequest);
});

add_test(function test_invalid_type() {
  do_check_throws(function() {
    Service.getStorageInfo("invalid", function(error, info) {
      do_throw("Shouldn't get here!");
    });
  });
  run_next_test();
});

add_test(function test_network_error() {
  Service.getStorageInfo(INFO_COLLECTIONS, function(error, info) {
    do_check_eq(error.result, Cr.NS_ERROR_CONNECTION_REFUSED);
    do_check_eq(info, null);
    run_next_test();
  });
});

add_test(function test_http_error() {
  let handler = httpd_handler(500, "Oh noez", "Something went wrong!");
  let server = httpd_setup({"/1.1/johndoe/info/collections": handler});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";

  let request = Service.getStorageInfo(INFO_COLLECTIONS, function(error, info) {
    do_check_eq(error.status, 500);
    do_check_eq(info, null);
    server.stop(run_next_test);
  });
});

add_test(function test_invalid_json() {
  let handler = httpd_handler(200, "OK", "Invalid JSON");
  let server = httpd_setup({"/1.1/johndoe/info/collections": handler});
  Service.serverURL = server.baseURI + "/";
  Service.clusterURL = server.baseURI + "/";

  let request = Service.getStorageInfo(INFO_COLLECTIONS, function(error, info) {
    do_check_eq(error.name, "SyntaxError");
    do_check_eq(info, null);
    server.stop(run_next_test);
  });
});
