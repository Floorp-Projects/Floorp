/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const gDOMWindowUtils = EventUtils._getDOMWindowUtils(window);

function promiseURLBarFocus() {
  const waitForFocusInURLBar = BrowserTestUtils.waitForEvent(gURLBar, "focus");
  gURLBar.blur();
  gURLBar.focus();
  return Promise.all([
    waitForFocusInURLBar,
    TestUtils.waitForCondition(
      () =>
        gDOMWindowUtils.IMEStatus === Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED &&
        gDOMWindowUtils.inputContextOrigin ===
          Ci.nsIDOMWindowUtils.INPUT_CONTEXT_ORIGIN_MAIN
    ),
  ]);
}

function promiseIMEStateEnabledByRemote() {
  return TestUtils.waitForCondition(
    () =>
      gDOMWindowUtils.IMEStatus === Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED &&
      gDOMWindowUtils.inputContextOrigin ===
        Ci.nsIDOMWindowUtils.INPUT_CONTEXT_ORIGIN_CONTENT
  );
}

async function test_url_bar_url(aDesc) {
  await promiseURLBarFocus();

  is(
    gDOMWindowUtils.inputContextURI,
    null,
    `When the search bar has focus, input context URI should be null because of in chrome document (${aDesc})`
  );
}

async function test_input_in_http_or_https(aIsHTTPS) {
  await promiseURLBarFocus();

  const scheme = aIsHTTPS ? "https" : "http";
  const url = `${scheme}://example.com/browser/toolkit/content/tests/browser/file_empty.html`;
  await BrowserTestUtils.withNewTab(url, async browser => {
    ok(browser.isRemoteBrowser, "This test passes only in e10s mode");

    await SpecialPowers.spawn(browser, [], async () => {
      content.document.body.innerHTML = "<input>";
      const input = content.document.querySelector("input");
      input.focus();

      // Wait for a tick for flushing IMEContentObserver's pending notifications.
      await new Promise(resolve =>
        content.requestAnimationFrame(() =>
          content.requestAnimationFrame(resolve)
        )
      );
    });

    await promiseIMEStateEnabledByRemote();
    if (!gDOMWindowUtils.inputContextURI) {
      ok(
        false,
        `Input context should have valid URI when the scheme of focused tab's URL is ${scheme}`
      );
      return;
    }
    is(
      gDOMWindowUtils.inputContextURI.spec,
      url,
      `Input context should have the document URI when the scheme of focused tab's URL is ${scheme}`
    );
  });
}

add_task(async () => {
  await test_url_bar_url("first check");
});
add_task(async () => {
  await test_input_in_http_or_https(true);
});
add_task(async () => {
  await test_url_bar_url("check after remote content sets the URI");
});
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", false]],
  });
  await test_input_in_http_or_https(false);
});

add_task(async function test_input_in_data() {
  await BrowserTestUtils.withNewTab("data:text/html,<input>", async browser => {
    ok(browser.isRemoteBrowser, "This test passes only in e10s mode");

    await SpecialPowers.spawn(browser, [], async () => {
      const input = content.document.querySelector("input");
      input.focus();

      // Wait for a tick for flushing IMEContentObserver's pending notifications.
      await new Promise(resolve =>
        content.requestAnimationFrame(() =>
          content.requestAnimationFrame(resolve)
        )
      );
    });

    await promiseIMEStateEnabledByRemote();
    is(
      gDOMWindowUtils.inputContextURI,
      null,
      "Input context should not have data URI"
    );
  });
});

add_task(async function test_omit_private_things_in_URL() {
  await SpecialPowers.pushPrefEnv({
    set: [["network.auth.confirmAuth.enabled", false]],
  });
  await promiseURLBarFocus();

  await BrowserTestUtils.withNewTab(
    "https://username:password@example.com/browser/toolkit/content/tests/browser/file_empty.html?query=some#ref",
    async browser => {
      ok(browser.isRemoteBrowser, "This test passes only in e10s mode");

      await SpecialPowers.spawn(browser, [], async () => {
        content.document.body.innerHTML = "<input>";
        const input = content.document.querySelector("input");
        input.focus();

        // Wait for a tick for flushing IMEContentObserver's pending notifications.
        await new Promise(resolve =>
          content.requestAnimationFrame(() =>
            content.requestAnimationFrame(resolve)
          )
        );
      });

      await promiseIMEStateEnabledByRemote();
      if (!gDOMWindowUtils.inputContextURI) {
        ok(
          false,
          `Input context should have valid URI even when the URL contains some private things`
        );
        return;
      }
      is(
        gDOMWindowUtils.inputContextURI.spec,
        "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
        `Input context should have the document URI which omit some private things in the URL`
      );
    }
  );
});
