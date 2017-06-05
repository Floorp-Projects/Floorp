/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

function login_handling(handler) {
  return function(request, response) {
    if (has_hawk_header(request)) {
      handler(request, response);
    } else {
      let body = "Unauthorized";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

function service_unavailable(request, response) {
  let body = "Service Unavailable";
  response.setStatusLine(request.httpVersion, 503, "Service Unavailable");
  response.setHeader("Retry-After", "42");
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  run_next_test();
}

add_task(async function test_verifyLogin() {
  // This test expects a clean slate -- no saved passphrase.
  Services.logins.removeAllLogins();
  let johnHelper = track_collections_helper();
  let johnU      = johnHelper.with_updated_collection;

  do_test_pending();

  let server = httpd_setup({
    "/1.1/johndoe/info/collections": login_handling(johnHelper.handler),
    "/1.1/janedoe/info/collections": service_unavailable,

    "/1.1/johndoe/storage/crypto/keys": johnU("crypto", new ServerWBO("keys").handler()),
    "/1.1/johndoe/storage/meta/global": johnU("meta", new ServerWBO("global").handler())
  });

  try {
    _("Force the initial state.");
    Service.status.service = STATUS_OK;
    do_check_eq(Service.status.service, STATUS_OK);

    _("Credentials won't check out because we're not configured yet.");
    Service.status.resetSync();
    do_check_false((await Service.verifyLogin()));
    do_check_eq(Service.status.service, CLIENT_NOT_CONFIGURED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NO_USERNAME);

    _("Success if syncBundleKey is set.");
    Service.status.resetSync();
    await configureIdentity({ username: "johndoe" }, server);
    do_check_true((await Service.verifyLogin()));
    do_check_eq(Service.status.service, STATUS_OK);
    do_check_eq(Service.status.login, LOGIN_SUCCEEDED);

    _("If verifyLogin() encounters a server error, it flips on the backoff flag and notifies observers on a 503 with Retry-After.");
    Service.status.resetSync();
    await configureIdentity({ username: "janedoe" }, server);
    Service._updateCachedURLs();
    do_check_false(Service.status.enforceBackoff);
    let backoffInterval;
    Svc.Obs.add("weave:service:backoff:interval", function observe(subject, data) {
      Svc.Obs.remove("weave:service:backoff:interval", observe);
      backoffInterval = subject;
    });
    do_check_false((await Service.verifyLogin()));
    do_check_true(Service.status.enforceBackoff);
    do_check_eq(backoffInterval, 42);
    do_check_eq(Service.status.service, LOGIN_FAILED);
    do_check_eq(Service.status.login, SERVER_MAINTENANCE);

    _("Ensure a network error when finding the cluster sets the right Status bits.");
    Service.status.resetSync();
    Service.clusterURL = "";
    Service._clusterManager._findCluster = () => "http://localhost:12345/";
    do_check_false((await Service.verifyLogin()));
    do_check_eq(Service.status.service, LOGIN_FAILED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NETWORK_ERROR);

    _("Ensure a network error when getting the collection info sets the right Status bits.");
    Service.status.resetSync();
    Service.clusterURL = "http://localhost:12345/";
    do_check_false((await Service.verifyLogin()));
    do_check_eq(Service.status.service, LOGIN_FAILED);
    do_check_eq(Service.status.login, LOGIN_FAILED_NETWORK_ERROR);

  } finally {
    Svc.Prefs.resetBranch("");
    server.stop(do_test_finished);
  }
});
