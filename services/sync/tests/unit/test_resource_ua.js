/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

var httpProtocolHandler = Cc["@mozilla.org/network/protocol;1?name=http"]
                          .getService(Ci.nsIHttpProtocolHandler);

// Tracking info/collections.
var collectionsHelper = track_collections_helper();
var collections = collectionsHelper.collections;

var meta_global;
var server;

var expectedUA;
var ua;
function uaHandler(f) {
  return function(request, response) {
    ua = request.getHeader("User-Agent");
    return f(request, response);
  };
}

add_task(async function setup() {

  Log.repository.rootLogger.addAppender(new Log.DumpAppender());
  meta_global = new ServerWBO('global');
  server = httpd_setup({
    "/1.1/johndoe/info/collections": uaHandler(collectionsHelper.handler),
    "/1.1/johndoe/storage/meta/global": uaHandler(meta_global.handler()),
  });

  await configureIdentity({ username: "johndoe" }, server);
  _("Server URL: " + server.baseURI);

  // Note this string is missing the trailing ".destkop" as the test
  // adjusts the "client.type" pref where that portion comes from.
  expectedUA = Services.appinfo.name + "/" + Services.appinfo.version +
               " (" + httpProtocolHandler.oscpu + ")" +
               " FxSync/" + WEAVE_VERSION + "." +
               Services.appinfo.appBuildID;

})

add_test(function test_fetchInfo() {
  _("Testing _fetchInfo.");
  Service.login();
  Service._fetchInfo();
  _("User-Agent: " + ua);
  do_check_eq(ua, expectedUA + ".desktop");
  ua = "";
  run_next_test();
});

add_test(function test_desktop_post() {
  _("Testing direct Resource POST.");
  let r = new AsyncResource(server.baseURI + "/1.1/johndoe/storage/meta/global");
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
  let r = new AsyncResource(server.baseURI + "/1.1/johndoe/storage/meta/global");
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
  let r = new AsyncResource(server.baseURI + "/1.1/johndoe/storage/meta/global");
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

