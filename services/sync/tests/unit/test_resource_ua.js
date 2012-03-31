Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/service.js");

const TEST_GET_URL = "http://localhost:8080/1.1/johndoe/storage/meta/global";

function test_resource_user_agent() {
  let meta_global = new ServerWBO('global');

  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let collections = collectionsHelper.collections;

  let ua;
  function uaHandler(f) {
    return function(request, response) {
      ua = request.getHeader("User-Agent");
      return f(request, response);
    };
  }

  do_test_pending();
  let server = httpd_setup({
    "/1.1/johndoe/info/collections": uaHandler(collectionsHelper.handler),
    "/1.1/johndoe/storage/meta/global": uaHandler(meta_global.handler()),
  });

  setBasicCredentials("johndoe", "ilovejane");
  Weave.Service.serverURL  = TEST_SERVER_URL;
  Weave.Service.clusterURL = TEST_CLUSTER_URL;

  let expectedUA = Services.appinfo.name + "/" + Services.appinfo.version +
                   " FxSync/" + WEAVE_VERSION + "." +
                   Services.appinfo.appBuildID;

  function test_fetchInfo(next) {
    _("Testing _fetchInfo.");
    Weave.Service._fetchInfo();
    _("User-Agent: " + ua);
    do_check_eq(ua, expectedUA + ".desktop");
    ua = "";
    next();
  }

  function test_desktop_post(next) {
    _("Testing direct Resource POST.");
    let r = new AsyncResource(TEST_GET_URL);
    r.post("foo=bar", function (error, content) {
      _("User-Agent: " + ua);
      do_check_eq(ua, expectedUA + ".desktop");
      ua = "";
      next();
    });
  }

  function test_desktop_get(next) {
    _("Testing async.");
    Svc.Prefs.set("client.type", "desktop");
    let r = new AsyncResource(TEST_GET_URL);
    r.get(function(error, content) {
      _("User-Agent: " + ua);
      do_check_eq(ua, expectedUA + ".desktop");
      ua = "";
      next();
    });
  }

  function test_mobile_get(next) {
    _("Testing mobile.");
    Svc.Prefs.set("client.type", "mobile");
    let r = new AsyncResource(TEST_GET_URL);
    r.get(function (error, content) {
      _("User-Agent: " + ua);
      do_check_eq(ua, expectedUA + ".mobile");
      ua = "";
      next();
    });
  }

  Async.chain(
    test_fetchInfo,
    test_desktop_post,
    test_desktop_get,
    test_mobile_get,
    function (next) {
      server.stop(next);
    },
    do_test_finished)();
}

function run_test() {
  test_resource_user_agent();
}
