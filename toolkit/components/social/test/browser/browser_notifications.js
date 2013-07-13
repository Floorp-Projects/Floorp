/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PROVIDER_ORIGIN = 'http://example.com';

Cu.import("resource://gre/modules/Services.jsm");

function ensureProvider(workerFunction, cb) {
  let manifest = {
    origin: TEST_PROVIDER_ORIGIN,
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/echo.sjs?"
               + encodeURI("let run=" + workerFunction.toSource()) + ";run();"
  };

  ensureSocialEnabled();
  SocialService.addProvider(manifest, function (p) {
    p.enabled = true;
    cb(p);
  });
}

function test() {
  waitForExplicitFinish();

  let cbPostTest = function(cb) {
    SocialService.removeProvider(TEST_PROVIDER_ORIGIN, function() {cb()});
  };
  replaceAlertsService();
  registerCleanupFunction(restoreAlertsService);
  runTests(tests, undefined, cbPostTest);
}

let tests = {
  testNotificationCallback: function(cbnext) {
    let run = function() {
      let testPort, apiPort;
      onconnect = function(e) {
        let port = e.ports[0];
        port.onmessage = function(e) {
          if (e.data.topic == "social.initialize") { // this is the api port.
            apiPort = port;
          } else if (e.data.topic == "test.initialize") { // test suite port.
            testPort = port;
            apiPort.postMessage({topic: 'social.notification-create',
                                 data: {
                                        id: "the id",
                                        body: 'test notification',
                                        action: 'callback',
                                        actionArgs: { data: "something" }
                                       }
                                });
          } else if (e.data.topic == "social.notification-action") {
            let data = e.data.data;
            let ok = data && data.action == "callback" &&
                     data.actionArgs && e.data.data.actionArgs.data == "something";
            testPort.postMessage({topic: "test.done", data: ok});
          }
        }
      }
    }
    ensureProvider(run, function(provider) {
      let observer = {
        observedData: null,
        observe: function(subject, topic, data) {
          this.observedData = JSON.parse(data);
          Services.obs.removeObserver(observer, "social-test:notification-alert");
        }
      }
      Services.obs.addObserver(observer, "social-test:notification-alert", false);

      let port = provider.getWorkerPort();
      ok(port, "got port from worker");
      port.onmessage = function(e) {
        if (e.data.topic == "test.done") {
          ok(e.data.data, "check the test worked");
          ok(observer.observedData, "test observer fired");
          is(observer.observedData.text, "test notification", "check the alert text is correct");
          is(observer.observedData.title, "Example Provider", "check the alert title is correct");
          is(observer.observedData.textClickable, true, "check the alert is clickable");
          port.close();
          cbnext();
        }
      }
      port.postMessage({topic: "test.initialize"});
    });
  }
};
