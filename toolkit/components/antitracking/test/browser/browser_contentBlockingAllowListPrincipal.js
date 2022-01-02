/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_SANDBOX_URL =
  "https://example.com/browser/toolkit/components/antitracking/test/browser/sandboxed.html";

/**
 * Tests the contentBlockingAllowListPrincipal.
 * @param {Browser} browser - Browser to test.
 * @param {("content"|"system")} type - Expected principal type.
 * @param {String} [origin] - Expected origin of principal. Defaults to the
 * origin of the browsers content principal.
 */
function checkAllowListPrincipal(
  browser,
  type,
  origin = browser.contentPrincipal.origin
) {
  let principal =
    browser.browsingContext.currentWindowGlobal
      .contentBlockingAllowListPrincipal;
  ok(principal, "Principal is set");

  if (type == "content") {
    ok(principal.isContentPrincipal, "Is content principal");

    ok(
      principal.schemeIs("https"),
      "allowlist content principal must have https scheme"
    );
  } else if (type == "system") {
    ok(principal.isSystemPrincipal, "Is system principal");
  } else {
    throw new Error("Unexpected principal type");
  }

  is(principal.origin, origin, "Correct origin");
}

/**
 * Runs a given test in a normal window and in a private browsing window.
 * @param {String} initialUrl - URL to load in the initial test tab.
 * @param {Function} testCallback - Test function to run in both windows.
 */
async function runTestInNormalAndPrivateMode(initialUrl, testCallback) {
  for (let i = 0; i < 2; i++) {
    let isPrivateBrowsing = !!i;
    info("Running test. Private browsing: " + !!i);
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: isPrivateBrowsing,
    });
    let tab = BrowserTestUtils.addTab(win.gBrowser, initialUrl);
    let browser = tab.linkedBrowser;

    await BrowserTestUtils.browserLoaded(browser);

    await testCallback(browser, isPrivateBrowsing);

    await BrowserTestUtils.closeWindow(win);
  }
}

/**
 * Creates an iframe in the passed browser and waits for it to load.
 * @param {Browser} browser - Browser to create the frame in.
 * @param {String} src - Frame source url.
 * @param {String} id - Frame id.
 * @param {String} [sandboxAttr] - Optional list of sandbox attributes to set
 * for the iframe. Defaults to no sandbox.
 * @returns {Promise} - Resolves once the frame has loaded.
 */
function createFrame(browser, src, id, sandboxAttr) {
  return SpecialPowers.spawn(
    browser,
    [{ page: src, frameId: id, sandboxAttr }],
    async function(obj) {
      await new content.Promise(resolve => {
        let frame = content.document.createElement("iframe");
        frame.src = obj.page;
        frame.id = obj.frameId;
        if (obj.sandboxAttr) {
          frame.setAttribute("sandbox", obj.sandboxAttr);
        }
        frame.addEventListener("load", resolve, { once: true });
        content.document.body.appendChild(frame);
      });
    }
  );
}

add_task(async setup => {
  // Disable heuristics. We don't need them and if enabled the resulting
  // telemetry can race with the telemetry in the next test.
  // See Bug 1686836, Bug 1686894.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.restrict3rdpartystorage.heuristic.redirect", false],
      ["privacy.restrict3rdpartystorage.heuristic.recently_visited", false],
      ["privacy.restrict3rdpartystorage.heuristic.window_open", false],
      ["dom.security.https_first_pbm", false],
    ],
  });
});

/**
 * Test that we get the correct allow list principal which matches the content
 * principal for an https site.
 */
add_task(async test_contentPrincipalHTTPS => {
  await runTestInNormalAndPrivateMode("https://example.com", browser => {
    checkAllowListPrincipal(browser, "content");
  });
});

/**
 * Tests that the scheme of the allowlist principal is HTTPS, even though the
 * site is loaded via HTTP.
 */
add_task(async test_contentPrincipalHTTP => {
  await runTestInNormalAndPrivateMode(
    "http://example.net",
    (browser, isPrivateBrowsing) => {
      checkAllowListPrincipal(
        browser,
        "content",
        "https://example.net" +
          (isPrivateBrowsing ? "^privateBrowsingId=1" : "")
      );
    }
  );
});

/**
 * Tests that the allow list principal is a system principal for the preferences
 * about site.
 */
add_task(async test_systemPrincipal => {
  await runTestInNormalAndPrivateMode("about:preferences", browser => {
    checkAllowListPrincipal(browser, "system");
  });
});

/**
 * Tests that we get a valid content principal for top level sandboxed pages,
 * and not the document principal which is a null principal.
 */
add_task(async test_TopLevelSandbox => {
  await runTestInNormalAndPrivateMode(
    TEST_SANDBOX_URL,
    (browser, isPrivateBrowsing) => {
      ok(
        browser.contentPrincipal.isNullPrincipal,
        "Top level sandboxed page should have null principal"
      );
      checkAllowListPrincipal(
        browser,
        "content",
        "https://example.com" +
          (isPrivateBrowsing ? "^privateBrowsingId=1" : "")
      );
    }
  );
});

/**
 * Tests that we get a valid content principal for a new tab opened via
 * window.open.
 */
add_task(async test_windowOpen => {
  await runTestInNormalAndPrivateMode("https://example.com", async browser => {
    checkAllowListPrincipal(browser, "content");

    let promiseTabOpened = BrowserTestUtils.waitForNewTab(
      browser.ownerGlobal.gBrowser,
      "https://example.org/",
      true
    );

    // Call window.open from iframe.
    await SpecialPowers.spawn(browser, [], async function() {
      content.open("https://example.org/");
    });

    let tab = await promiseTabOpened;

    checkAllowListPrincipal(tab.linkedBrowser, "content");

    BrowserTestUtils.removeTab(tab);
  });
});

/**
 * Tests that we get a valid content principal for a new tab opened via
 * window.open from a sandboxed iframe.
 */
add_task(async test_windowOpenFromSandboxedFrame => {
  await runTestInNormalAndPrivateMode(
    "https://example.com",
    async (browser, isPrivateBrowsing) => {
      checkAllowListPrincipal(browser, "content");

      // Create sandboxed iframe, allow popups.
      await createFrame(
        browser,
        "https://example.com",
        "sandboxedIframe",
        "allow-popups"
      );
      // Iframe BC is the only child of the test browser.
      let [frameBrowsingContext] = browser.browsingContext.children;

      let promiseTabOpened = BrowserTestUtils.waitForNewTab(
        browser.ownerGlobal.gBrowser,
        "https://example.org/",
        true
      );

      // Call window.open from iframe.
      await SpecialPowers.spawn(frameBrowsingContext, [], async function() {
        content.open("https://example.org/");
      });

      let tab = await promiseTabOpened;

      checkAllowListPrincipal(
        tab.linkedBrowser,
        "content",
        "https://example.org" +
          (isPrivateBrowsing ? "^privateBrowsingId=1" : "")
      );

      BrowserTestUtils.removeTab(tab);
    }
  );
});
