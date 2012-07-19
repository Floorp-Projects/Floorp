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

  ensureSocialEnabled();

  SocialService.addProvider(manifest, function (p) {
    provider = p;
    runTests(tests, undefined, undefined, function () {
      SocialService.removeProvider(provider.origin, finish);
    });
  });
}

let tests = {
  testInitializeWorker: function(next) {
    ok(provider.workerAPI, "provider has a workerAPI");
    is(provider.workerAPI.initialized, false, "workerAPI is not yet initialized");
  
    let port = provider.port;
    ok(port, "should be able to get a port from the provider");
  
    port.onmessage = function onMessage(event) {
      let {topic, data} = event.data;
      if (topic == "test-initialization-complete") {
        is(provider.workerAPI.initialized, true, "workerAPI is now initialized");
        next();
      }
    }
    port.postMessage({topic: "test-initialization"});
  },

  testProfile: function(next) {
    let expect = {
      portrait: "chrome://branding/content/icon48.png",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:profile-changed", false);
      is(aData, provider.origin, "update of profile from our provider");
      let profile = provider.profile;
      is(profile.portrait, expect.portrait, "portrait is set");
      is(profile.userName, expect.userName, "userName is set");
      is(profile.displayName, expect.displayName, "displayName is set");
      is(profile.profileURL, expect.profileURL, "profileURL is set");

      next();
    }
    Services.obs.addObserver(ob, "social:profile-changed", false);
    provider.workerAPI._port.postMessage({topic: "test-profile", data: expect});
  },

  testAmbientNotification: function(next) {
    let expect = {
      name: "test-ambient"
    }
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:ambient-notification-changed", false);
      is(aData, provider.origin, "update is from our provider");
      let notif = provider.ambientNotificationIcons[expect.name];
      is(notif.name, expect.name, "ambientNotification reflected");

      next();
    }
    Services.obs.addObserver(ob, "social:ambient-notification-changed", false);
    provider.workerAPI._port.postMessage({topic: "test-ambient", data: expect});
  },

  testProfileCleared: function(next) {
    let sent = {
      userName: ""
    };
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:profile-changed", false);
      is(aData, provider.origin, "update of profile from our provider");
      is(Object.keys(provider.profile).length, 0, "profile was cleared by empty username");
      is(Object.keys(provider.ambientNotificationIcons).length, 0, "icons were cleared by empty username");

      next();
    }
    Services.obs.addObserver(ob, "social:profile-changed", false);
    provider.workerAPI._port.postMessage({topic: "test-profile", data: sent});
  }
};
