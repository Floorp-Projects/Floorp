Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function do_check_throws(func) {
  var raised = false;
  try {
    func();
  } catch (ex) {
    raised = true;
  }
  do_check_true(raised);
}

function send(statusCode, status, body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, statusCode, status);
    response.bodyOutputStream.write(body, body.length);
  };
}

function test_findCluster() {
  _("Test Weave.Service._findCluster()");
  let server;
  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";

    _("_findCluster() throws on network errors (e.g. connection refused).");
    do_check_throws(function() {
      Weave.Service._findCluster();
    });

    server = httpd_setup({
      "/user/1.0/johndoe/node/weave": send(200, "OK", "http://weave.user.node/"),
      "/user/1.0/jimdoe/node/weave": send(200, "OK", "null"),
      "/user/1.0/janedoe/node/weave": send(404, "Not Found", "Not Found"),
      "/user/1.0/juliadoe/node/weave": send(400, "Bad Request", "Bad Request"),
      "/user/1.0/joedoe/node/weave": send(500, "Server Error", "Server Error")
    });

    _("_findCluster() returns the user's cluster node");
    let cluster = Weave.Service._findCluster();
    do_check_eq(cluster, "http://weave.user.node/");

    _("A 'null' response is converted to null.");
    Weave.Service.username = "jimdoe";
    cluster = Weave.Service._findCluster();
    do_check_eq(cluster, null);

    _("If a 404 is encountered, the server URL is taken as the cluster URL");
    Weave.Service.username = "janedoe";
    cluster = Weave.Service._findCluster();
    do_check_eq(cluster, Weave.Service.serverURL);

    _("A 400 response will throw an error.");
    Weave.Service.username = "juliadoe";
    do_check_throws(function() {
      Weave.Service._findCluster();
    });

    _("Any other server response (e.g. 500) will throw an error.");
    Weave.Service.username = "joedoe";
    do_check_throws(function() {
      Weave.Service._findCluster();
    });

  } finally {
    Svc.Prefs.resetBranch("");
    if (server) {
      server.stop(function() {});
    }
  }
}


function test_setCluster() {
  _("Test Weave.Service._setCluster()");
  let server = httpd_setup({
    "/user/1.0/johndoe/node/weave": send(200, "OK", "http://weave.user.node/"),
    "/user/1.0/jimdoe/node/weave": send(200, "OK", "null")
  });
  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";

    _("Check initial state.");
    do_check_eq(Weave.Service.clusterURL, "");

    _("Set the cluster URL.");
    do_check_true(Weave.Service._setCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.user.node/");

    _("Setting it again won't make a difference if it's the same one.");
    do_check_false(Weave.Service._setCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.user.node/");

    _("A 'null' response won't make a difference either.");
    Weave.Service.username = "jimdoe";
    do_check_false(Weave.Service._setCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.user.node/");      

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}

function test_updateCluster() {
  _("Test Weave.Service._updateCluster()");
  let server = httpd_setup({
    "/user/1.0/johndoe/node/weave": send(200, "OK", "http://weave.user.node/"),
    "/user/1.0/janedoe/node/weave": send(200, "OK", "http://weave.cluster.url/")
  });
  try {
    Weave.Service.serverURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";

    _("Check initial state.");
    do_check_eq(Weave.Service.clusterURL, "");
    do_check_eq(Svc.Prefs.get("lastClusterUpdate"), null);

    _("Set the cluster URL.");
    do_check_true(Weave.Service._updateCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.user.node/");
    let lastUpdate = parseInt(Svc.Prefs.get("lastClusterUpdate"), 10);
    do_check_true(lastUpdate > Date.now() - 1000);

    _("Trying to update the cluster URL within the backoff timeout won't do anything.");
    do_check_false(Weave.Service._updateCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.user.node/");
    do_check_eq(parseInt(Svc.Prefs.get("lastClusterUpdate"), 10), lastUpdate);

    _("Time travel 30 mins into the past and the update will work.");
    Weave.Service.username = "janedoe";
    Svc.Prefs.set("lastClusterUpdate", (lastUpdate - 30*60*1000).toString());

    do_check_true(Weave.Service._updateCluster());
    do_check_eq(Weave.Service.clusterURL, "http://weave.cluster.url/");
    lastUpdate = parseInt(Svc.Prefs.get("lastClusterUpdate"), 10);
    do_check_true(lastUpdate > Date.now() - 1000);
  
  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(function() {});
  }
}

function run_test() {
  test_findCluster();
  test_setCluster();
  test_updateCluster();
}
