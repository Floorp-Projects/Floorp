/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = {
    origin: 'http://example.com',
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/worker_social.js"
  };

  ensureSocialEnabled();

  SocialService.addProvider(manifest, function (provider) {
    ok(provider.workerAPI, "provider has a workerAPI");
    is(provider.workerAPI.initialized, false, "workerAPI is not yet initialized");
  
    let port = provider.port;
    ok(port, "should be able to get a port from the provider");
  
    port.onmessage = function onMessage(event) {
      let {topic, data} = event.data;
      if (topic == "test-initialization-complete") {
        is(provider.workerAPI.initialized, true, "workerAPI is now initialized");
        SocialService.removeProvider(provider.origin, finish);
      }
    }
    port.postMessage({topic: "test-initialization"});
  });
}
