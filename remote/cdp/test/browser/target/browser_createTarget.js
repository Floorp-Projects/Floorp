/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_TEST =
  "https://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

add_task(
  async function raisesWithoutArguments({ client }) {
    const { Target } = client;

    let exceptionThrown = false;
    try {
      await Target.createTarget();
    } catch (e) {
      exceptionThrown = true;
    }
    ok(exceptionThrown, "createTarget raised error without a URL");
  },
  { createTab: false }
);

add_task(
  async function raisesWithInvalidUrlType({ client }) {
    const { Target } = client;

    for (const url of [null, true, 1, [], {}]) {
      info(`Checking url with invalid value: ${url}`);

      let errorThrown = "";
      try {
        await Target.createTarget({
          url,
        });
      } catch (e) {
        errorThrown = e.message;
      }

      ok(
        errorThrown.match(/url: string value expected/),
        `URL fails for invalid type: ${url}`
      );
    }
  },
  { createTab: false }
);

add_task(
  async function invalidUrlDefaults({ client }) {
    const { Target } = client;
    const expectedUrl = "about:blank";

    for (const url of ["", "example.com", "https://example[.com", "https:"]) {
      const { targetId } = await Target.createTarget({ url });
      is(typeof targetId, "string", "Got expected type for target id");

      const browser = gBrowser.selectedTab.linkedBrowser;
      await BrowserTestUtils.browserLoaded(browser, false, expectedUrl);

      is(browser.currentURI.spec, expectedUrl, "Expected URL loaded");
    }
  },
  { createTab: false }
);

add_task(
  async function opensTabWithCorrectInfo({ client }) {
    const { Target } = client;

    const url = PAGE_TEST;
    const { targetId } = await Target.createTarget({ url });

    is(typeof targetId, "string", "Got expected type for target id");

    const browser = gBrowser.selectedTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(browser, false, url);

    is(browser.currentURI.spec, url, "Expected URL loaded");

    const { targetInfos } = await Target.getTargets();
    const targetInfo = targetInfos.find(info => info.targetId === targetId);
    ok(!!targetInfo, "Found target info with the same target id");
    is(targetInfo.url, url, "Target info refers to the same target URL");
    is(
      targetInfo.type,
      "page",
      "Target info refers to the same target as page type"
    );
    ok(
      !targetInfo.attached,
      "Target info refers to the same target as not attached"
    );
  },
  { createTab: false }
);
