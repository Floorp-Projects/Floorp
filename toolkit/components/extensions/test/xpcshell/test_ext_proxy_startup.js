"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

let { promiseRestartManager, promiseShutdownManager, promiseStartupManager } =
  AddonTestUtils;

let nonProxiedRequests = 0;
const nonProxiedServer = createHttpServer({ hosts: ["example.com"] });
nonProxiedServer.registerPathHandler("/", (request, response) => {
  nonProxiedRequests++;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

// No hosts defined to avoid proxy filter setup.
let proxiedRequests = 0;
const server = createHttpServer();
server.identity.add("http", "proxied.example.com", 80);
server.registerPathHandler("/", (request, response) => {
  proxiedRequests++;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-script-event", "start-background-script"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

add_setup(() => {
  // In case the prefs have a different value by default.
  Services.prefs.setBoolPref(
    "extensions.webextensions.early_background_wakeup_on_request",
    false
  );
});

// Test that a proxy listener during startup does not immediately
// start the background page, but the event is queued until the background
// page is started.
add_task(async function test_proxy_startup() {
  await promiseStartupManager();

  function background(proxyInfo) {
    browser.proxy.onRequest.addListener(
      details => {
        // ignore speculative requests
        if (details.type == "xmlhttprequest") {
          browser.test.sendMessage("saw-request");
        }
        return proxyInfo;
      },
      { urls: ["<all_urls>"] }
    );
  }

  let proxyInfo = {
    host: server.identity.primaryHost,
    port: server.identity.primaryPort,
    type: "http",
  };

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      permissions: ["proxy", "http://proxied.example.com/*"],
    },
    background: `(${background})(${JSON.stringify(proxyInfo)})`,
  });

  await extension.startup();

  // Initial requests to test the proxy and non-proxied servers.
  await Promise.all([
    extension.awaitMessage("saw-request"),
    ExtensionTestUtils.fetch("http://proxied.example.com/?a=0"),
  ]);
  equal(1, proxiedRequests, "proxied request ok");
  equal(0, nonProxiedRequests, "non proxied request ok");

  await ExtensionTestUtils.fetch("http://example.com/?a=0");
  equal(1, proxiedRequests, "proxied request ok");
  equal(1, nonProxiedRequests, "non proxied request ok");

  await promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();

  let events = trackEvents(extension);

  // Initiate a non-proxied request to make sure the startup listeners are using
  // the extensions filters/etc.
  await ExtensionTestUtils.fetch("http://example.com/?a=1");
  equal(1, proxiedRequests, "proxied request ok");
  equal(2, nonProxiedRequests, "non proxied request ok");

  equal(
    events.get("background-script-event"),
    false,
    "Should not have gotten a background script event"
  );

  // Make a request that the extension will proxy once it is started.
  let request = Promise.all([
    extension.awaitMessage("saw-request"),
    ExtensionTestUtils.fetch("http://proxied.example.com/?a=1"),
  ]);

  await promiseExtensionEvent(extension, "background-script-event");
  equal(
    events.get("background-script-event"),
    true,
    "Should have gotten a background script event"
  );

  // Test the background page startup.
  equal(
    events.get("start-background-script"),
    false,
    "Should not have started the background page yet"
  );
  AddonTestUtils.notifyEarlyStartup();
  await new Promise(executeSoon);

  equal(
    events.get("start-background-script"),
    true,
    "Should have gotten a background script event"
  );

  // Verify our proxied request finishes properly and that the
  // request was not handled via our non-proxied server.
  await request;
  equal(2, proxiedRequests, "proxied request ok");
  equal(2, nonProxiedRequests, "non proxied requests ok");

  // Retry, but now with early_background_wakeup_on_request=true.
  Services.prefs.setBoolPref(
    "extensions.webextensions.early_background_wakeup_on_request",
    true
  );
  await promiseRestartManager({ earlyStartup: false });
  await extension.awaitStartup();
  let request2 = Promise.all([
    extension.awaitMessage("saw-request"),
    ExtensionTestUtils.fetch("http://proxied.example.com/?a=2"),
  ]);
  info("Expecting background page to be awakened by the proxy event");
  await extension.awaitBackgroundStarted();
  await request2;
  equal(3, proxiedRequests, "proxied request ok");

  await extension.unload();

  await promiseShutdownManager();
});
