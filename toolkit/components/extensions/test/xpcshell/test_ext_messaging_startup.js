"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

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

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  true
);

const PAGE_HTML = `<!DOCTYPE html><meta charset="utf-8"><script src="script.js"></script>`;

function trackEvents(wrapper) {
  let events = new Map();
  for (let event of ["background-page-event", "start-background-page"]) {
    events.set(event, false);
    wrapper.extension.once(event, () => events.set(event, true));
  }
  return events;
}

async function test(what, background, script) {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",

    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/*"],
          js: ["script.js"],
        },
      ],
    },

    files: {
      "page.html": PAGE_HTML,
      "script.js": script,
    },

    background,
  });

  info(`Set up ${what} listener`);
  await extension.startup();
  await extension.awaitMessage("bg-ran");

  info(`Test wakeup for ${what} from an extension page`);
  await promiseRestartManager();
  await extension.awaitStartup();

  function awaitBgEvent() {
    return new Promise(resolve =>
      extension.extension.once("background-page-event", resolve)
    );
  }

  let events = trackEvents(extension);

  let url = extension.extension.baseURI.resolve("page.html");

  let [, page] = await Promise.all([
    awaitBgEvent(),
    ExtensionTestUtils.loadContentPage(url, { extension }),
  ]);

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

  equal(extension.messageQueue.size, 0, "Have not yet received bg-ran message");

  let promise = extension.awaitMessage("bg-ran");
  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  await promise;

  equal(
    events.get("start-background-page"),
    true,
    "Should have gotten start-background-page event"
  );

  await extension.awaitFinish("messaging-test");
  ok(true, "Background page loaded and received message from extension page");

  await page.close();

  info(`Test wakeup for ${what} from a content script`);
  ExtensionParent._resetStartupPromises();
  await promiseRestartManager();
  await extension.awaitStartup();

  events = trackEvents(extension);

  [, page] = await Promise.all([
    awaitBgEvent(),
    ExtensionTestUtils.loadContentPage(
      "http://example.com/data/file_sample.html"
    ),
  ]);

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

  equal(extension.messageQueue.size, 0, "Have not yet received bg-ran message");

  promise = extension.awaitMessage("bg-ran");
  Services.obs.notifyObservers(null, "browser-delayed-startup-finished");
  await promise;

  equal(
    events.get("start-background-page"),
    true,
    "Should have gotten start-background-page event"
  );

  await extension.awaitFinish("messaging-test");
  ok(true, "Background page loaded and received message from content script");

  await page.close();
  await extension.unload();

  await promiseShutdownManager();
  ExtensionParent._resetStartupPromises();
}

add_task(function test_onMessage() {
  function script() {
    browser.runtime.sendMessage("ping").then(reply => {
      browser.test.assertEq(
        reply,
        "pong",
        "Extension page received pong reply"
      );
      browser.test.notifyPass("messaging-test");
    });
  }

  async function background() {
    browser.runtime.onMessage.addListener((msg, sender) => {
      browser.test.assertEq(
        msg,
        "ping",
        "Background page received ping message"
      );
      return Promise.resolve("pong");
    });

    // addListener() returns right away but make a round trip to the
    // main process to ensure the persistent onMessage listener is recorded.
    await browser.runtime.getBrowserInfo();
    browser.test.sendMessage("bg-ran");
  }

  return test("onMessage", background, script);
});

add_task(function test_onConnect() {
  function script() {
    let port = browser.runtime.connect();
    port.onMessage.addListener(msg => {
      browser.test.assertEq(msg, "pong", "Extension page received pong reply");
      browser.test.notifyPass("messaging-test");
    });
    port.postMessage("ping");
  }

  async function background() {
    browser.runtime.onConnect.addListener(port => {
      port.onMessage.addListener(msg => {
        browser.test.assertEq(
          msg,
          "ping",
          "Background page received ping message"
        );
        port.postMessage("pong");
      });
    });

    // addListener() returns right away but make a round trip to the
    // main process to ensure the persistent onMessage listener is recorded.
    await browser.runtime.getBrowserInfo();
    browser.test.sendMessage("bg-ran");
  }

  return test("onConnect", background, script);
});

// Test that messaging works if the background page is started before
// any messages are exchanged.  (See bug 1467136 for an example of how
// this broke at one point).
add_task(async function test_other_startup() {
  await promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",

    async background() {
      browser.runtime.onMessage.addListener(msg => {
        browser.test.notifyPass("startup");
      });

      // addListener() returns right away but make a round trip to the
      // main process to ensure the persistent onMessage listener is recorded.
      await browser.runtime.getBrowserInfo();
      browser.test.sendMessage("bg-ran");
    },

    files: {
      "page.html": PAGE_HTML,
      "script.js"() {
        browser.runtime.sendMessage("ping");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg-ran");

  await promiseRestartManager();
  await extension.awaitStartup();

  // Start the background page.  No message have been sent at this point.
  Services.obs.notifyObservers(null, "sessionstore-windows-restored");
  await extension.awaitMessage("bg-ran");

  // Now that the background page is fully started, load a new page that
  // sends a message to the background page.
  let url = extension.extension.baseURI.resolve("page.html");
  let page = await ExtensionTestUtils.loadContentPage(url, { extension });

  await extension.awaitFinish("startup");

  await page.close();
  await extension.unload();

  await promiseShutdownManager();
  ExtensionParent._resetStartupPromises();
});
