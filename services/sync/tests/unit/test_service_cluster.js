/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function do_check_throws(func) {
  var raised = false;
  try {
    func();
  } catch (ex) {
    raised = true;
  }
  do_check_true(raised);
}

add_test(function test_findCluster() {
  _("Test Service._findCluster()");
  let server;
  ensureLegacyIdentityManager();
  try {
    _("_findCluster() throws on network errors (e.g. connection refused).");
    do_check_throws(function() {
      Service.serverURL = "http://dummy:9000/";
      Service.identity.account = "johndoe";
      Service._clusterManager._findCluster();
    });

    server = httpd_setup({
      "/user/1.0/johndoe/node/weave": httpd_handler(200, "OK", "http://weave.user.node/"),
      "/user/1.0/jimdoe/node/weave": httpd_handler(200, "OK", "null"),
      "/user/1.0/janedoe/node/weave": httpd_handler(404, "Not Found", "Not Found"),
      "/user/1.0/juliadoe/node/weave": httpd_handler(400, "Bad Request", "Bad Request"),
      "/user/1.0/joedoe/node/weave": httpd_handler(500, "Server Error", "Server Error")
    });

    Service.serverURL = server.baseURI;
    Service.identity.account = "johndoe";

    _("_findCluster() returns the user's cluster node");
    let cluster = Service._clusterManager._findCluster();
    do_check_eq(cluster, "http://weave.user.node/");

    _("A 'null' response is converted to null.");
    Service.identity.account = "jimdoe";
    cluster = Service._clusterManager._findCluster();
    do_check_eq(cluster, null);

    _("If a 404 is encountered, the server URL is taken as the cluster URL");
    Service.identity.account = "janedoe";
    cluster = Service._clusterManager._findCluster();
    do_check_eq(cluster, Service.serverURL);

    _("A 400 response will throw an error.");
    Service.identity.account = "juliadoe";
    do_check_throws(function() {
      Service._clusterManager._findCluster();
    });

    _("Any other server response (e.g. 500) will throw an error.");
    Service.identity.account = "joedoe";
    do_check_throws(function() {
      Service._clusterManager._findCluster();
    });

  } finally {
    Svc.Prefs.resetBranch("");
    if (server) {
      server.stop(run_next_test);
    }
  }
});

add_test(function test_setCluster() {
  _("Test Service._setCluster()");
  let server = httpd_setup({
    "/user/1.0/johndoe/node/weave": httpd_handler(200, "OK", "http://weave.user.node/"),
    "/user/1.0/jimdoe/node/weave": httpd_handler(200, "OK", "null")
  });
  try {
    Service.serverURL = server.baseURI;
    Service.identity.account = "johndoe";

    _("Check initial state.");
    do_check_eq(Service.clusterURL, "");

    _("Set the cluster URL.");
    do_check_true(Service._clusterManager.setCluster());
    do_check_eq(Service.clusterURL, "http://weave.user.node/");

    _("Setting it again won't make a difference if it's the same one.");
    do_check_false(Service._clusterManager.setCluster());
    do_check_eq(Service.clusterURL, "http://weave.user.node/");

    _("A 'null' response won't make a difference either.");
    Service.identity.account = "jimdoe";
    do_check_false(Service._clusterManager.setCluster());
    do_check_eq(Service.clusterURL, "http://weave.user.node/");

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(run_next_test);
  }
});

function run_test() {
  initTestLogging();
  run_next_test();
}
