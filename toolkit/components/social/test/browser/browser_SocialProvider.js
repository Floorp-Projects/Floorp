/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let provider;

function test() {
  waitForExplicitFinish();

  let manifest = {
    origin: 'http://example.com',
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/worker_social.js"
  };

  SocialService.addProvider(manifest, function (p) {
    provider = p;
    runTests(tests, undefined, undefined, function () {
      SocialService.removeProvider(p.origin, function() {
        ok(!provider.enabled, "removing an enabled provider should have disabled the provider");
        let port = provider.getWorkerPort();
        ok(!port, "should not be able to get a port after removing the provider");
        provider = null;
        finish();
      });
    });
  });
}

let tests = {
  testSingleProvider: function(next) {
    ok(provider.enabled, "provider is initially enabled");
    let port = provider.getWorkerPort();
    ok(port, "should be able to get a port from enabled provider");
    port.close();
    ok(provider.workerAPI, "should be able to get a workerAPI from enabled provider");

    provider.enabled = false;

    ok(!provider.enabled, "provider is now disabled");
    port = provider.getWorkerPort();
    ok(!port, "shouldn't be able to get a port from disabled provider");
    ok(!provider.workerAPI, "shouldn't be able to get a workerAPI from disabled provider");

    provider.enabled = true;

    ok(provider.enabled, "provider is re-enabled");
    let port = provider.getWorkerPort();
    ok(port, "should be able to get a port from re-enabled provider");
    port.close();
    ok(provider.workerAPI, "should be able to get a workerAPI from re-enabled provider");
    next();
  },
  testTwoProviders: function(next) {
    // add another provider, test both workers
    let manifest = {
      origin: 'http://test2.example.com',
      name: "Example Provider 2",
      workerURL: "http://test2.example.com/browser/toolkit/components/social/test/browser/worker_social.js"
    };
    SocialService.addProvider(manifest, function (provider2) {
      ok(provider.enabled, "provider is initially enabled");
      ok(provider2.enabled, "provider2 is initially enabled");
      let port = provider.getWorkerPort();
      let port2 = provider2.getWorkerPort();
      ok(port, "have port for provider");
      ok(port2, "have port for provider2");
      port.onmessage = function(e) {
        if (e.data.topic == "test-initialization-complete") {
          ok(true, "first provider initialized");
          port2.postMessage({topic: "test-initialization"});
        }
      }
      port2.onmessage = function(e) {
        if (e.data.topic == "test-initialization-complete") {
          ok(true, "second provider initialized");
          SocialService.removeProvider(provider2.origin, function() {
            next();
          });
        }
      }
      port.postMessage({topic: "test-initialization"});
    });
  }
}
