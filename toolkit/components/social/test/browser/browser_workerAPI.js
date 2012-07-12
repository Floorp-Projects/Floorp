/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialProvider = Components.utils.import("resource://gre/modules/SocialProvider.jsm", {}).SocialProvider;

function test() {
  waitForExplicitFinish();

  // This test creates a SocialProvider object directly - it would be nicer to
  // go through the SocialService, but adding a test provider before the service
  // has been initialized can be tricky.
  let provider = new SocialProvider({
    origin: 'http://example.com',
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/worker_social.js"
  });

  ok(provider.workerAPI, "provider has a workerAPI");
  is(provider.workerAPI.initialized, false, "workerAPI is not yet initialized");

  let port = provider.getWorkerPort();
  ok(port, "should be able to get a port from the provider");

  port.onmessage = function onMessage(event) {
    let {topic, data} = event.data;
    if (topic == "test-initialization-complete") {
      is(provider.workerAPI.initialized, true, "workerAPI is now initialized");
      provider.terminate();
      finish();
    }
  }
  port.postMessage({topic: "test-initialization"});
}
