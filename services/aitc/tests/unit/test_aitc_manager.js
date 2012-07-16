/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Webapps.jsm");

Cu.import("resource://services-aitc/client.js");
Cu.import("resource://services-aitc/manager.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/preferences.js");

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

// Check that a is less than b.
function do_check_lt(a, b) {
  do_check_true(a < b);
}

add_test(function test_manager_localapps() {
  // Install two fake apps into the DOM registry.
  let fakeApp1 = get_mock_app();
  fakeApp1.manifest = {
    name: "Appasaurus 1",
    description: "One of the best fake apps ever",
    launch_path: "/",
    fullscreen: true,
    required_features: ["webgl"]
  };

  let fakeApp2 = get_mock_app();
  fakeApp2.manifest = {
    name: "Appasaurus 2",
    description: "The other best fake app ever",
    launch_path: "/",
    fullscreen: true,
    required_features: ["geolocation"]
  };

  DOMApplicationRegistry.confirmInstall({app: fakeApp1});
  DOMApplicationRegistry.confirmInstall({app: fakeApp2});

  // Create an instance of the manager and check if it put the app in the queue.
  // We put doInitialUpload in nextTick, because maanger will not be defined
  // in the callback. This pattern is used everywhere, AitcManager is created.
  let manager = new AitcManager(function() {
    CommonUtils.nextTick(doInitialUpload);
  });

  function doInitialUpload() {
    manager.initialSchedule(function(num) {
      // 2 apps should have been queued.
      do_check_eq(num, 2);
      do_check_eq(manager._pending.length, 2);

      let entry = manager._pending.peek();
      do_check_eq(entry.type, "install");
      do_check_eq(entry.app.origin, fakeApp1.origin);

      // Remove one app from queue.
      manager._pending.dequeue(run_next_test);
    });
  }
});

add_test(function test_manager_alreadysynced() {
  // The manager should ignore any local apps if we've already synced before.
  Preferences.set("services.aitc.client.lastModified", "" + Date.now());

  let manager = new AitcManager(function() {
    CommonUtils.nextTick(doCheck);
  });

  function doCheck() {
    manager.initialSchedule(function(num) {
      do_check_eq(num, -1);
      do_check_eq(manager._pending.length, 1);
      // Clear queue for next test.
      manager._pending.dequeue(run_next_test);
    });
  }
});

add_test(function test_401_responses() {
  PREFS.set("client.backoff", "50");
  PREFS.set("manager.putFreq", 50);
  const app = get_mock_app();
  const username = "123";
  const premadeToken = {
    id: "testtest",
    key: "testtest",
    endpoint: "http://localhost:8080/1.0/123",
    uid: "uid",
    duration: "5000"
  };

  let server = get_server_with_user(username);
  let client = get_client_for_server(username, server);

  server.mockStatus = {
    code: 401,
    method: "Unauthorized"
  };

  let mockRequestCount = 0;
  let clientFirstToken = null;
  server.onRequest = function mockstatus() {
    mockRequestCount++;
    switch (mockRequestCount) {
      case 1:
        clientFirstToken = client.token;
        // Switch to using mock 201s.
        this.mockStatus = {
          code: 201,
          method: "Created"
        };
        break;
      case 2:
        // Check that the client obtained a different token.
        do_check_neq(client.token.id, clientFirstToken.id);
        do_check_neq(client.token.key, clientFirstToken.key);
        server.stop(run_next_test);
        break;
    }
  }

  let manager = new AitcManager(function() {
    CommonUtils.nextTick(gotManager);
  }, client, premadeToken);

  function gotManager() {
    // Assume first token is not outdated.
    manager._lastTokenTime = Date.now();
    manager.appEvent("install", get_mock_app());
  }
});

add_test(function test_client_exponential_backoff() {
  _("Test that the client is properly setting the backoff");

  // Use prefs to speed up tests.
  const putFreq = 50;
  const initialBackoff = 50;
  const username = "123";
  PREFS.set("manager.putFreq", putFreq);
  PREFS.set("client.backoff.initial", initialBackoff);
  PREFS.set("client.backoff.max", 100000);

  let mockRequestCount = 0;
  let lastRequestTime = Date.now();
  // Create server that returns failure codes.
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
    // initial backoff.
    if (mockRequestCount === 4) {
      do_check_lt(initialBackoff, timeDiff);
    // The time beween the 4th and 5th request should be atleast double
    // the intial backoff.
    } else if (mockRequestCount === 5) {
      do_check_lt(initialBackoff * 2, timeDiff);
      server.stop(run_next_test);
    }
  }

  // Create dummy client and manager.
  let client = get_client_for_server(username, server);
  let manager = new AitcManager(function() {
    CommonUtils.nextTick(gotManager);
  }, client);

  function gotManager() {
    manager._lastTokenTime = Date.now();
    // Create a bunch of dummy apps for the queue to cycle through.
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