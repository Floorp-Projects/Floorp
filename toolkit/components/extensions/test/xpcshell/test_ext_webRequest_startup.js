"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

let {
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  true
);

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-page-event", "start-background-page"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

// Test that a non-blocking listener during startup does not immediately
// start the background page, but the event is queued until the background
// page is started.
add_task(async function test_1() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["webRequest", "http://example.com/"],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("saw-request");
        },
        { urls: ["http://example.com/data/file_sample.html"] }
      );
    },
  });

  await extension.startup();

  await promiseRestartManager();
  await extension.awaitStartup();

  let events = trackEvents(extension);

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  equal(
    events.get("background-page-event"),
    true,
    "Should have gotten a background page event"
  );
  equal(
    events.get("start-background-page"),
    false,
    "Background page should not be started"
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  await new Promise(executeSoon);

  equal(
    events.get("start-background-page"),
    true,
    "Should have gotten start-background-page event"
  );

  await extension.awaitMessage("saw-request");
  ok(true, "Background page loaded and received webRequest event");

  await extension.unload();

  await promiseShutdownManager();
});

// Tests that filters are handled properly: if we have a blocking listener
// with a filter, a request that does not match the filter does not get
// suspended and does not start the background page.
add_task(async function test_2() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "http://test1.example.com/",
      ],
    },

    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.fail("Listener should not have been called");
        },
        { urls: ["http://test1.example.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await promiseRestartManager();
  await extension.awaitStartup();

  let events = trackEvents(extension);

  await ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  equal(
    events.get("background-page-event"),
    false,
    "Should not have gotten a background page event"
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  await new Promise(executeSoon);

  equal(
    events.get("start-background-page"),
    false,
    "Should not have tried to start background page yet"
  );

  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  await extension.awaitMessage("ready");

  await extension.unload();
  await promiseShutdownManager();
});

// Test that a blocking listener that uses filterResponseData() works
// properly (i.e., that the delayed call to registerTraceableChannel
// works properly).
add_task(async function test_3() {
  const DATA = `<!DOCTYPE html>
<html>
<body>
  <h1>This is a modified page</h1>
</body>
</html>`;

  function background(data) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        let filter = browser.webRequest.filterResponseData(details.requestId);
        filter.onstop = () => {
          let encoded = new TextEncoder("utf-8").encode(data);
          filter.write(encoded);
          filter.close();
        };
      },
      { urls: ["http://example.com/data/file_sample.html"] },
      ["blocking"]
    );
  }

  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "http://example.com/"],
    },

    background: `(${background})(${uneval(DATA)})`,
  });

  await extension.startup();

  await promiseRestartManager();
  await extension.awaitStartup();

  let dataPromise = ExtensionTestUtils.fetch(
    "http://example.com/",
    "http://example.com/data/file_sample.html"
  );

  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  let data = await dataPromise;

  equal(
    data,
    DATA,
    "Stream filter was properly installed for a load during startup"
  );

  await extension.unload();
  await promiseShutdownManager();
});
