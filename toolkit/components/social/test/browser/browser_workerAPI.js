/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let provider;

function test() {
  waitForExplicitFinish();

  replaceAlertsService();
  registerCleanupFunction(restoreAlertsService);

  let manifest = {
    origin: 'http://example.com',
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/worker_social.js"
  };

  ensureSocialEnabled();

  SocialService.addProvider(manifest, function (p) {
    p.enabled = true;
    provider = p;
    runTests(tests, undefined, undefined, function () {
      SocialService.removeProvider(provider.origin, finish);
    });
  });
}

let tests = {
  testProfile: function(next) {
    let expect = {
      portrait: "https://example.com/portrait.jpg",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:profile-changed");
      is(aData, provider.origin, "update of profile from our provider");
      let profile = provider.profile;
      is(profile.portrait, expect.portrait, "portrait is set");
      is(profile.userName, expect.userName, "userName is set");
      is(profile.displayName, expect.displayName, "displayName is set");
      is(profile.profileURL, expect.profileURL, "profileURL is set");
      next();
    }
    Services.obs.addObserver(ob, "social:profile-changed", false);
    let port = provider.getWorkerPort();
    port.postMessage({topic: "test-profile", data: expect});
    port.close();
  },

  testAmbientNotification: function(next) {
    let expect = {
      name: "test-ambient"
    }
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:ambient-notification-changed");
      is(aData, provider.origin, "update is from our provider");
      let notif = provider.ambientNotificationIcons[expect.name];
      is(notif.name, expect.name, "ambientNotification reflected");

      next();
    }
    Services.obs.addObserver(ob, "social:ambient-notification-changed", false);
    let port = provider.getWorkerPort();
    port.postMessage({topic: "test-ambient", data: expect});
    port.close();
  },

  testProfileCleared: function(next) {
    let sent = {
      userName: ""
    };
    function ob(aSubject, aTopic, aData) {
      Services.obs.removeObserver(ob, "social:profile-changed");
      is(aData, provider.origin, "update of profile from our provider");
      is(Object.keys(provider.profile).length, 0, "profile was cleared by empty username");
      is(Object.keys(provider.ambientNotificationIcons).length, 0, "icons were cleared by empty username");

      next();
    }
    Services.obs.addObserver(ob, "social:profile-changed", false);
    provider.workerAPI._port.postMessage({topic: "test-profile", data: sent});
  },

  testNoCookies: function(next) {
    // use a big, blunt stick to remove cookies.
    Services.cookies.removeAll();
    let port = provider.getWorkerPort();
    port.onmessage = function onMessage(event) {
      let {topic, data} = event.data;
      if (topic == "test.cookies-get-response") {
        is(data.length, 0, "got no cookies");
        port.close();
        next();
      }
    }
    port.postMessage({topic: "test-initialization"});
    port.postMessage({topic: "test.cookies-get"});
  },

  testCookies: function(next) {
    let port = provider.getWorkerPort();
    port.onmessage = function onMessage(event) {
      let {topic, data} = event.data;
      if (topic == "test.cookies-get-response") {
        is(data.length, 2, "got 2 cookies");
        is(data[0].name, "cheez", "cookie has the correct name");
        is(data[0].value, "burger", "cookie has the correct value");
        is(data[1].name, "moar", "cookie has the correct name");
        is(data[1].value, "bacon", "cookie has the correct value");
        Services.cookies.remove('.example.com', '/', 'cheez', false);
        Services.cookies.remove('.example.com', '/', 'moar', false);
        port.close();
        next();
      }
    }
    var MAX_EXPIRY = Math.pow(2, 62);
    Services.cookies.add('.example.com', '/', 'cheez', 'burger', false, false, true, MAX_EXPIRY);
    Services.cookies.add('.example.com', '/', 'moar', 'bacon', false, false, true, MAX_EXPIRY);
    port.postMessage({topic: "test-initialization"});
    port.postMessage({topic: "test.cookies-get"});
  },

  testWorkerReload: function(next) {
    let fw = {};
    Cu.import("resource://gre/modules/FrameWorker.jsm", fw);

    // get a real handle to the worker so we can watch the unload event
    // we watch for the unload of the worker to know it is infact being
    // unloaded, after that if we get worker.connected we know that
    // the worker was loaded again and ports reconnected.

    // Given our <browser remote="true"> model, we have to some of this in a
    // content script.  We turn this function into a data: URL...
    // (and note that this code only makes sense in the context of
    //  FrameWorkerContent.js...)
    let contentScript = function() {
      // the content script may be executed while there are pending messages,
      // such as ports from the previous test being closed, which screws us up.
      // By only doing this work on a message, we ensure previous messages have
      // all been delivered.
      addMessageListener("frameworker-test:ready", function onready() {
        removeMessageListener("frameworker-test:ready", onready);
        // first, find our test's port - it will be the last one.
        let port, id
        for ([id, port] of frameworker.ports) {
          ; // nothing to do - we just want to get the last...
        }
        let unloadHasFired = false,
            haveNoPendingMessagesBefore = true,
            havePendingMessagesAfter = false;
        content.addEventListener("unload", function workerUnload(e) {
          content.removeEventListener("unload", workerUnload);
          // There should be no "pending" messages with one subtle exception -
          // there *might* be a social.initialize message - the reload process
          // posts a message to do the reload, then posts the init message - it
          // may or maynot have arrived here by now - but there certainly should
          // be no other messages.
          for (let [temp_id, temp_port] of frameworker.ports) {
            if (temp_port._pendingMessagesOutgoing.length == 0)
              continue;
            if (temp_port._pendingMessagesOutgoing.length == 1 &&
                temp_port._pendingMessagesOutgoing[0].data &&
                JSON.parse(temp_port._pendingMessagesOutgoing[0].data).topic == "social.initialize")
              continue;
            // we found something we aren't expecting...
            haveNoPendingMessagesBefore = false;
          }
          unloadHasFired = true; // at the end, so errors above aren't masked.
        });
        addEventListener("DOMWindowCreated", function workerLoaded(e) {
          removeEventListener("DOMWindowCreated", workerLoaded);
          // send a message which should end up pending
          port.postMessage({topic: "test-pending-msg"});
          for (let [temp_id, temp_port] of frameworker.ports) {
            if (temp_port._pendingMessagesOutgoing.length >= 0) {
              havePendingMessagesAfter = true;
            }
          }
          let ok = unloadHasFired && haveNoPendingMessagesBefore && havePendingMessagesAfter;
          sendAsyncMessage("test-result", {ok: ok});
        });
      });
    };

    let reloading = false;
    let worker = fw.getFrameWorkerHandle(provider.workerURL, undefined, "testWorkerReload");
    let port = provider.getWorkerPort();
    worker._worker.browserPromise.then(browser => {
      browser.messageManager.loadFrameScript("data:," + encodeURI(contentScript.toSource()) + "();", false);
      browser.messageManager.sendAsyncMessage("frameworker-test:ready");
      let seenTestResult = false;
      browser.messageManager.addMessageListener("test-result", function _onTestResult(msg) {
        browser.messageManager.removeMessageListener("test-result", _onTestResult);
        ok(msg.json.ok, "test result from content-script is ok");
        seenTestResult = true;
      });

      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-initialization-complete":
            // tell the worker to send the reload msg - that will trigger the
            // frameworker to unload and for our content script's unload
            // handler to post a "test-pending" message, which in-turn causes
            // the worker to send the test-pending-response.
            // All of which goes to prove that if a message is delivered while
            // the frameworker is unloaded due to a reload, that message still
            // gets delivered.
            port.postMessage({topic: "test-reload-init"});
            break;
          case "test-pending-response":
            ok(e.data.data.seenInit, "worker has seen the social.initialize message");
            // now we've been reloaded, check that our worker is still the same
            let newWorker = fw.getFrameWorkerHandle(provider.workerURL, undefined, "testWorkerReload");
            is(worker._worker, newWorker._worker, "worker is the same after reload");
            ok(true, "worker reloaded and testPort was reconnected");
            ok(seenTestResult, "saw result message from content");
            next();
            break;
        }
      }
      port.postMessage({topic: "test-initialization"});
    });
  },

  testNotificationLinks: function(next) {
    let port = provider.getWorkerPort();
    let data = {
      id: 'an id',
      body: 'the text',
      action: 'link',
      actionArgs: {} // will get a toURL elt during the tests...
    }
    let testArgs = [
      // toURL,                 expectedLocation,     expectedWhere]
      ["http://example.com",      "http://example.com/", "tab"],
      // bug 815970 - test that a mis-matched scheme gets patched up.
      ["https://example.com",     "http://example.com/", "tab"],
      // check an off-origin link is not opened.
      ["https://mochitest:8888/", null,                 null]
    ];

    // we monkey-patch openUILinkIn
    let oldopenUILinkIn = window.openUILinkIn;
    registerCleanupFunction(function () {
      // restore the monkey-patch
      window.openUILinkIn = oldopenUILinkIn;
    });
    let openLocation;
    let openWhere;
    window.openUILinkIn = function(location, where) {
      openLocation = location;
      openWhere = where;
    }

    // the testing framework.
    let toURL, expectedLocation, expectedWhere;
    function nextTest() {
      if (testArgs.length == 0) {
        port.close();
        next(); // all out of tests!
        return;
      }
      openLocation = openWhere = null;
      [toURL, expectedLocation, expectedWhere] = testArgs.shift();
      data.actionArgs.toURL = toURL;
      port.postMessage({topic: 'test-notification-create', data: data});
    };

    port.onmessage = function(evt) {
      if (evt.data.topic == "did-notification-create") {
        is(openLocation, expectedLocation, "url actually opened was " + openLocation);
        is(openWhere, expectedWhere, "the url was opened in a " + expectedWhere);
        nextTest();
      }
    }
    // and kick off the tests.
    port.postMessage({topic: "test-initialization"});
    nextTest();
  },

};
