/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const TEST_GET_URL = "http://localhost:8080/1.1/johndoe/storage/meta/global";

// Tracking info/collections.
let collectionsHelper = track_collections_helper();
let collections = collectionsHelper.collections;

let meta_global;
let server;

let expectedUA;
let ua;
function uaHandler(f) {
  return function(request, response) {
    ua = request.getHeader("User-Agent");
    return f(request, response);
  };
}

function run_test() {
  meta_global = new ServerWBO('global');
  server = httpd_setup({
    "/1.1/johndoe/info/collections": uaHandler(collectionsHelper.handler),
    "/1.1/johndoe/storage/meta/global": uaHandler(meta_global.handler()),
  });

  setBasicCredentials("johndoe", "ilovejane");
  Service.serverURL  = TEST_SERVER_URL;
  Service.clusterURL = TEST_CLUSTER_URL;

  expectedUA = Services.appinfo.name + "/" + Services.appinfo.version +
               " FxSync/" + WEAVE_VERSION + "." +
               Services.appinfo.appBuildID;

  run_next_test();
}

add_test(function test_fetchInfo() {
  _("Testing _fetchInfo.");
  Service._fetchInfo();
  _("User-Agent: " + ua);
  do_check_eq(ua, expectedUA + ".desktop");
  ua = "";
  run_next_test();
});

add_test(function test_desktop_post() {
  _("Testing direct Resource POST.");
  let r = new AsyncResource(TEST_GET_URL);
  r.post("foo=bar", function (error, content) {
    _("User-Agent: " + ua);
    do_check_eq(ua, expectedUA + ".desktop");
    ua = "";
    run_next_test();
  });
});

add_test(function test_desktop_get() {
  _("Testing async.");
  Svc.Prefs.set("client.type", "desktop");
  let r = new AsyncResource(TEST_GET_URL);
  r.get(function(error, content) {
    _("User-Agent: " + ua);
    do_check_eq(ua, expectedUA + ".desktop");
    ua = "";
    run_next_test();
  });
});

add_test(function test_mobile_get() {
  _("Testing mobile.");
  Svc.Prefs.set("client.type", "mobile");
  let r = new AsyncResource(TEST_GET_URL);
  r.get(function (error, content) {
    _("User-Agent: " + ua);
    do_check_eq(ua, expectedUA + ".mobile");
    ua = "";
    run_next_test();
  });
});

add_test(function tear_down() {
  server.stop(run_next_test);
});

