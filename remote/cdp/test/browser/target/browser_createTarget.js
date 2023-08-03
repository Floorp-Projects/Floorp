/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_TEST =
  "https://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

add_task(
  async function raisesWithoutArguments({ client }) {
    const { Target } = client;

    await Assert.rejects(
      Target.createTarget(),
      err => err.message.includes("url: string value expected"),
      "createTarget raised error without a URL"
    );
  },
  { createTab: false }
);

add_task(
  async function raisesWithInvalidUrlType({ client }) {
    const { Target } = client;

    for (const url of [null, true, 1, [], {}]) {
      info(`Checking url with invalid value: ${url}`);

      await Assert.rejects(
        Target.createTarget({
          url,
        }),
        /url: string value expected/,
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
      // Here we cannot wait for browserLoaded, because the tab might already
      // be on about:blank when `createTarget` resolves.
      const onNewTabLoaded = BrowserTestUtils.waitForNewTab(
        gBrowser,
        "about:blank",
        true
      );
      const { targetId } = await Target.createTarget({ url });
      is(typeof targetId, "string", "Got expected type for target id");

      // Wait for the load to be done before checking the URL.
      const tab = await onNewTabLoaded;
      const browser = tab.linkedBrowser;
      is(browser.currentURI.spec, expectedUrl, "Expected URL loaded");
    }
  },
  { createTab: false }
);

add_task(
  async function opensTabWithCorrectInfo({ client }) {
    const { Target } = client;

    const url = PAGE_TEST;
    const onNewTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, url, true);
    const { targetId } = await Target.createTarget({ url });

    is(typeof targetId, "string", "Got expected type for target id");

    const tab = await onNewTabLoaded;
    const browser = tab.linkedBrowser;
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
