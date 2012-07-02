/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-aitc/client.js");
Cu.import("resource://services-aitc/manager.js");
Cu.import("resource://testing-common/services-common/aitcserver.js");

const PREFS = new Preferences("services.aitc.");

let count = 0;

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

function get_mock_app() {

  let app = {
    name: "Mocking Birds",
    origin: "http://example" + ++count + ".com",
    installOrigin: "http://example.com",
    installedAt: Date.now(),
    modifiedAt: Date.now(),
    receipts: [],
    manifestURL: "/manifest.webapp"
  };

  return app;
}

function get_mock_queue_element() {
  return {
    type: "install",
    app: get_mock_app(),
    retries: 0,
    lastTime: 0
  }
}

function get_client_for_server(username, server) {
  _("Create server user for User " + username);

  let token = {
    endpoint: server.url + username,
    id: "ID-HERE",
    key: "KEY-HERE"
  };

  return new AitcClient(token, new Preferences("services.aitc.client."));
}

// Check that a is less than b
function do_check_lt(a, b) {
  do_check_true(a < b);
}

add_test(function test_client_exponential_backoff() {
  _("Test that the client is properly setting the backoff");

  // Use prefs to speed up tests
  const putFreq = 50;
  const initialBackoff = 50;
  const username = "123";
  PREFS.set("manager.putFreq", putFreq);
  PREFS.set("client.backoff.initial", initialBackoff);
  PREFS.set("client.backoff.max", 100000);

  let mockRequestCount = 0;
  let lastRequestTime = Date.now();
  // Create server that returns failure codes
  let server = get_server_with_user(username);
  server.mockStatus = {
    code: 399,
    method: "Self Destruct"
  }

  server.onRequest = function onRequest() {
    mockRequestCount++;
    let timeDiff, timeNow = Date.now();
    if (mockRequestCount !== 1) {
      timeDiff = timeNow - lastRequestTime;
    }
    lastRequestTime = timeNow;

    // The time between the 3rd and 4th request should be atleast the
    // initial backoff
    if (mockRequestCount === 4) {
      do_check_lt(initialBackoff, timeDiff);
    // The time beween the 4th and 5th request should be atleast double
    // the intial backoff
    } else if (mockRequestCount === 5) {
      do_check_lt(initialBackoff * 2, timeDiff);
      server.stop(run_next_test);
    }
  }

  // Create dummy client and manager
  let client = get_client_for_server(username, server);
  let manager = new AitcManager(function() {
    CommonUtils.nextTick(gotManager);
  }, client);

  function gotManager() {
    manager._lastToken = new Date();
    // Create a bunch of dummy apps for the queue to cycle through
    manager._pending._queue = [
      get_mock_queue_element(),
      get_mock_queue_element(),
      get_mock_queue_element(),
      get_mock_queue_element(),
    ];
    // Add the dummy apps to the queue, then start the polling cycle with an app
    // event.
    manager._pending._putFile(manager._pending._queue, function (err) {
      manager.appEvent("install", get_mock_app());
    });
  }

});