/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

// TODO enable once build infra supports testing modules.
//Cu.import("resource://testing-common/services-common/aitcserver.js");

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

function get_aitc_server() {
  let server = new AITCServer10Server();
  server.start(get_server_port());

  return server;
}

function get_server_with_user(username) {
  let server = get_aitc_server();
  server.createUser(username);

  return server;
}

add_test(function test_origin_conversion() {
  let mapping = {
    "www.mozilla.org": "xSMmiFEpg4b4TRtzJZd6Mvy4hGc",
    "foo":             "C-7Hteo_D9vJXQ3UfzxbwnXaijM",
  };

  for (let k in mapping) {
    do_check_eq(AITCServer10User.prototype.originToID(k), mapping[k]);
  }

  run_next_test();
});

add_test(function test_empty_user() {
  _("Ensure user instances can be created.");

  let user = new AITCServer10User();

  let apps = user.getApps();
  do_check_eq([app for (app in apps)].length, 0);
  do_check_false(user.hasAppID("foobar"));

  run_next_test();
});

add_test(function test_user_add_app() {
  _("Ensure apps can be added to users.");

  let user = new AITCServer10User();
  let threw = false;
  try {
    user.addApp({});
  } catch (ex) {
    threw = true;
  } finally {
    do_check_true(threw);
    threw = false;
  }

  run_next_test();
});

add_test(function test_server_run() {
  _("Ensure server can be started properly.");

  let server = new AITCServer10Server();
  server.start(get_server_port());

  server.stop(run_next_test);
});

add_test(function test_create_user() {
  _("Ensure users can be created properly.");

  let server = get_aitc_server();

  let u1 = server.createUser("123");
  do_check_true(u1 instanceof AITCServer10User);

  let u2 = server.getUser("123");
  do_check_eq(u1, u2);

  server.stop(run_next_test);
});

add_test(function test_empty_server_404() {
  _("Ensure empty server returns 404.");

  let server = get_aitc_server();
  let request = new RESTRequest(server.url + "123/");
  request.get(function onComplete(error) {
    do_check_eq(this.response.status, 404);

    let request = new RESTRequest(server.url + "123/apps/");
    request.get(function onComplete(error) {
      do_check_eq(this.response.status, 404);

      server.stop(run_next_test);
    });
  });
});

add_test(function test_empty_user_apps() {
  _("Ensure apps request for empty user has appropriate content.");

  const username = "123";

  let server = get_server_with_user(username);
  let request = new RESTRequest(server.url + username + "/apps/");
  _("Performing request...");
  request.get(function onComplete(error) {
    _("Got response");
    do_check_eq(error, null);

    do_check_eq(200, this.response.status);
    let headers = this.response.headers;
    do_check_true("content-type" in headers);
    do_check_eq(headers["content-type"], "application/json");
    do_check_true("x-timestamp" in headers);

    let body = this.response.body;
    let parsed = JSON.parse(body);
    do_check_attribute_count(parsed, 1);
    do_check_true("apps" in parsed);
    do_check_true(Array.isArray(parsed.apps));
    do_check_eq(parsed.apps.length, 0);

    server.stop(run_next_test);
  });
});

add_test(function test_invalid_request_method() {
  _("Ensure HTTP 405 works as expected.");

  const username = "12345";

  let server = get_server_with_user(username);
  let request = new RESTRequest(server.url + username + "/apps/foobar");
  request.dispatch("SILLY", null, function onComplete(error) {
    do_check_eq(error, null);
    do_check_eq(this.response.status, 405);

    let headers = this.response.headers;
    do_check_true("accept" in headers);

    let allowed = new Set();

    for (let method of headers["accept"].split(",")) {
      allowed.add(method);
    }

    do_check_eq(allowed.size(), 3);
    for (let method of ["GET", "PUT", "DELETE"]) {
      do_check_true(allowed.has(method));
    }

    run_next_test();
  });
});
