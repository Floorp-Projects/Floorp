/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-aitc/client.js");

Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/preferences.js");

Cu.import("resource://testing-common/services-common/aitcserver.js");

const PREFS = new Preferences("services.aitc.client.")

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

function get_aitc_server() {
  _("Create new server.");

  let server = new AITCServer10Server();
  server.start(get_server_port());

  return server;
}

function get_server_with_user(username) {
  _("Create server user for User " + username);

  let server = get_aitc_server();
  server.createUser(username);

  return server;
}

function get_mock_app(remote) {

  let app = {
    name: "Mocking Birds",
    origin: "http://example.com",
    installOrigin: "http://example.com",
    installedAt: Date.now(),
    modifiedAt: Date.now(),
    receipts: []
  };

  app[remote ? 'manifestPath' : 'manifestURL'] = "/manifest.webapp";

  return app;
}

function get_client_for_server(username, server) {
  _("Create server user for User " + username);

  let token = {
    endpoint: server.url + username,
    id: 'ID-HERE',
    key: 'KEY-HERE'
  };

  let client = new AitcClient(token, PREFS);

  return client;
}

// Clean up code - backoff is preserved between requests in a pref
function advance(server) {
  PREFS.set("backoff", "0");
  if (server) {
    server.stop(run_next_test);
  } else {
    run_next_test();
  }
}

add_test(function test_getapps_empty() {
  _("Ensure client request for empty user has appropriate content.");

  const username = "123";

  let server = get_server_with_user(username);
  let client = get_client_for_server(username, server);

  client.getApps(function(error, apps) {
    _("Got response");
    do_check_null(error);

    do_check_true(Array.isArray(apps));
    do_check_eq(apps.length, 0);

    advance(server);
  });
});

add_test(function test_install_app() {
  _("Ensure client request for installing an app has appropriate content.");

  const username = "123";
  const app = get_mock_app();

  let server = get_server_with_user(username);

  let client = get_client_for_server(username, server);

  // TODO _putApp instead of, as install requires app in registry
  client._putApp(client._makeRemoteApp(app), function(error, status) {
    _("Got response");
    do_check_null(error);

    do_check_true(status);

    client.getApps(function(error, apps) {
      _("Got response");
      do_check_null(error);

      do_check_true(Array.isArray(apps));
      do_check_eq(apps.length, 1);

      let first = apps[0];

      do_check_eq(first.origin, app.origin);

      advance(server);
    });
  });
});

add_test(function test_uninstall_app() {
  _("Ensure client request for un-installing an app has appropriate content.");

  const username = "123";
  const app = get_mock_app();

  let server = get_server_with_user(username);
  let client = get_client_for_server(username, server);

  server.users[username].addApp(get_mock_app(true));

  client.remoteUninstall(app, function(error, status) {
    _("Got response");
    do_check_null(error);

    do_check_true(status);

    client.getApps(function(error, apps) {
      _("Got response");
      do_check_eq(error);

      do_check_true(Array.isArray(apps));
      do_check_eq(apps.length, 1);

      let first = apps[0];

      do_check_eq(first.origin, app.origin);
      do_check_true(first.hidden);

      advance(server);
    });
  });
});
