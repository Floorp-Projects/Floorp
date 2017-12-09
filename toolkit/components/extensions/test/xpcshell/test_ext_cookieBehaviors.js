"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ExtensionParent.jsm");

const {
  // cookieBehavior constants.
  BEHAVIOR_REJECT_FOREIGN,
  BEHAVIOR_REJECT,
  BEHAVIOR_LIMIT_FOREIGN,

  // lifetimePolicy constants.
  ACCEPT_SESSION,
} = Ci.nsICookieService;

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// Test that the indexedDB and localStorage are allowed in an extension background page
// and that the indexedDB is allowed in a extension worker.
async function test_bg_page_allowed_storage() {
  function background() {
    try {
      void indexedDB;
      void localStorage;

      const worker = new Worker("worker.js");
      worker.onmessage = (event) => {
        if (event.data.pass) {
          browser.test.notifyPass("bg_allowed_storage");
        } else {
          browser.test.notifyFail("bg_allowed_storage");
        }
      };

      worker.postMessage({});
    } catch (err) {
      browser.test.notifyFail("bg_allowed_storage");
      throw err;
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    files: {
      "worker.js": function worker() {
        this.onmessage = () => {
          try {
            void indexedDB;
            postMessage({pass: true});
          } catch (err) {
            postMessage({pass: false});
            throw err;
          }
        };
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("bg_allowed_storage");
  await extension.unload();
}

add_task(async function test_ext_page_allowed_storage_on_cookieBehaviors() {
  do_print("Test background page indexedDB with BEHAVIOR_LIMIT_FOREIGN");
  Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_LIMIT_FOREIGN);
  await test_bg_page_allowed_storage();

  do_print("Test background page indexedDB with BEHAVIOR_REJECT_FOREIGN");
  Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_REJECT_FOREIGN);
  await test_bg_page_allowed_storage();

  do_print("Test background page indexedDB with BEHAVIOR_REJECT");
  Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_REJECT);
  await test_bg_page_allowed_storage();
});

// Test that the webpage's indexedDB and localStorage are still not allowed from a content script
// when the cookie behavior, even when they are allowed in the extension pages.
add_task(async function test_content_script_on_cookieBehaviorReject() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", BEHAVIOR_REJECT);

  function contentScript() {
    // Ensure that when the current cookieBehavior doesn't allow a webpage to use indexedDB
    // or localStorage, then a WebExtension content script is not allowed to use it as well.
    browser.test.assertThrows(
      () => indexedDB,
      /The operation is insecure/,
      "a content script can't use indexedDB from a page where it is disallowed");

    browser.test.assertThrows(
      () => localStorage,
      /The operation is insecure/,
      "a content script can't use localStorage from a page where it is disallowed");

    browser.test.notifyPass("cs_disallowed_storage");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        matches: ["http://*/*/file_sample.html"],
        js: ["content_script.js"],
      }],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);

  await extension.awaitFinish("cs_disallowed_storage");

  await contentPage.close();
  await extension.unload();
});

add_task(function clear_cookieBehavior_pref() {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
});

// Test that localStorage is not in session-only mode for the extension pages,
// even when the session-only mode has been globally enabled.
add_task(async function test_localStorage_on_session_lifetimePolicy() {
  // localStorage in session-only mode.
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", ACCEPT_SESSION);

  function background() {
    localStorage.setItem("test-key", "test-value");

    browser.test.sendMessage("bg_localStorage_set", {uuid: window.location.hostname});
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  await extension.startup();
  const {uuid} = await extension.awaitMessage("bg_localStorage_set");

  const fakeAddonActor = {addonId: extension.id};
  const addonBrowser = await ExtensionParent.DebugUtils.getExtensionProcessBrowser(fakeAddonActor);
  const {isRemoteBrowser} = addonBrowser;

  const {
    isSessionOnly,
    domStorageLength,
    domStorageStoredValue,
  } = await ContentTask.spawn(addonBrowser, {uuid, isRemoteBrowser}, (params) => {
    const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});
    let windowEnumerator = Services.ww.getWindowEnumerator();

    let bgPageWindow;

    // Search the background page window in the process where the extension is running.
    while (windowEnumerator.hasMoreElements()) {
      let win = windowEnumerator.getNext();

      // When running in remote-webextension mode the window enumerator
      // will only include top level windows related to the extension process
      // (the background page and the "about:blank" related to the addonBrowser
      // used to connect to the right process).

      if (!params.isRemoteBrowser) {
        if (win.location.href !== "chrome://extensions/content/dummy.xul") {
          // When running in single process mode, all the top level windows
          // will be enumerated, we ignore any window that is not an
          // extension windowlessBrowser.
          continue;
        } else {
          // from an extension windowlessBrowser, retrieve the background page window
          // (which has been loaded inside a XUL browser element).
          win = win.document.querySelector("browser").contentWindow;
        }
      }

      if (win.location.hostname === params.uuid &&
          win.location.pathname === "/_generated_background_page.html") {
        // Once we have found the background page window related to the target extension
        // we can exit the while loop.
        bgPageWindow = win;
        break;
      }
    }

    if (!bgPageWindow) {
      throw new Error("Unable to find the extension background page");
    }

    return {
      isSessionOnly: bgPageWindow.localStorage.isSessionOnly,
      domStorageLength: bgPageWindow.localStorage.length,
      domStorageStoredValue: bgPageWindow.localStorage.getItem("test-key"),
    };
  });

  await ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(fakeAddonActor);

  equal(isSessionOnly, false, "the extension localStorage is not set in session-only mode");
  equal(domStorageLength, 1, "the extension storage contains the expected number of keys");
  equal(domStorageStoredValue, "test-value", "the extension storage contains the expected data");

  await extension.unload();
});

add_task(function clear_lifetimePolicy_pref() {
  Services.prefs.clearUserPref("network.cookie.lifetimePolicy");
});
