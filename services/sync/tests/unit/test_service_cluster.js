/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");

add_task(async function test_findCluster() {
  syncTestLogging();
  _("Test Service._findCluster()");
  try {
    let whenReadyToAuthenticate = PromiseUtils.defer();
    Service.identity.whenReadyToAuthenticate = whenReadyToAuthenticate;
    whenReadyToAuthenticate.resolve(true);

    Service.identity._ensureValidToken = () => Promise.reject(new Error("Connection refused"));

    _("_findCluster() throws on network errors (e.g. connection refused).");
    await Assert.rejects(Service._clusterManager._findCluster());

    Service.identity._ensureValidToken = () => Promise.resolve(true);
    Service.identity._token = { endpoint: "http://weave.user.node" };

    _("_findCluster() returns the user's cluster node");
    let cluster = await Service._clusterManager._findCluster();
    Assert.equal(cluster, "http://weave.user.node/");

  } finally {
    Svc.Prefs.resetBranch("");
  }
});

add_task(async function test_setCluster() {
  syncTestLogging();
  _("Test Service._setCluster()");
  try {
    _("Check initial state.");
    Assert.equal(Service.clusterURL, "");

    Service._clusterManager._findCluster = () => "http://weave.user.node/";

    _("Set the cluster URL.");
    Assert.ok((await Service._clusterManager.setCluster()));
    Assert.equal(Service.clusterURL, "http://weave.user.node/");

    _("Setting it again won't make a difference if it's the same one.");
    Assert.ok(!(await Service._clusterManager.setCluster()));
    Assert.equal(Service.clusterURL, "http://weave.user.node/");

    _("A 'null' response won't make a difference either.");
    Service._clusterManager._findCluster = () => null;
    Assert.ok(!(await Service._clusterManager.setCluster()));
    Assert.equal(Service.clusterURL, "http://weave.user.node/");
  } finally {
    Svc.Prefs.resetBranch("");
  }
});
