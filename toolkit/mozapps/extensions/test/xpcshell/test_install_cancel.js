/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

var testserver = createHttpServer({ hosts: ["example.com"] });

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1.9.2"
);

class TestListener {
  constructor(listener) {
    this.listener = listener;
  }

  onDataAvailable(...args) {
    this.origListener.onDataAvailable(...args);
  }

  onStartRequest(request) {
    this.origListener.onStartRequest(request);
  }

  onStopRequest(request, status) {
    if (this.listener.onStopRequest) {
      this.listener.onStopRequest(request, status);
    }
    this.origListener.onStopRequest(request, status);
  }
}

function startListener(listener) {
  let observer = {
    observe(subject, topic, data) {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (channel.URI.spec === "http://example.com/addons/test.xpi") {
        let channelListener = new TestListener(listener);
        channelListener.origListener = subject
          .QueryInterface(Ci.nsITraceableChannel)
          .setNewListener(channelListener);
        Services.obs.removeObserver(observer, "http-on-modify-request");
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-modify-request");
}

add_task(async function setup() {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Test",
      version: "1.0",
      applications: { gecko: { id: "cancel@test" } },
    },
  });
  testserver.registerFile(`/addons/test.xpi`, xpi);
  await AddonTestUtils.promiseStartupManager();
});

// This test checks that canceling an install after the download is completed fails
// and throws an exception as expected
add_task(async function test_install_cancelled() {
  let url = "http://example.com/addons/test.xpi";
  let install = await AddonManager.getInstallForURL(url, {
    name: "Test",
    version: "1.0",
  });

  let cancelInstall = new Promise(resolve => {
    startListener({
      onStopRequest() {
        resolve(Promise.resolve().then(() => install.cancel()));
      },
    });
  });

  await install.install().then(() => {
    ok(true, "install succeeded");
  });

  await cancelInstall
    .then(() => {
      ok(false, "cancel should not succeed");
    })
    .catch(e => {
      ok(!!e, "cancel threw an exception");
    });
});
