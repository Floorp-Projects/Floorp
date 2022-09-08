/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

/**
 * Check that policy allows certain extensions to be launched.
 */
add_task(async function test_download_jnlp_policy() {
  forcePromptForFiles("application/x-java-jnlp-file", "jnlp");
  let windowObserver = BrowserTestUtils.domWindowOpenedAndLoaded();

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "example.jnlp",
    waitForLoad: false,
  });
  let win = await windowObserver;

  let dialog = win.document.querySelector("dialog");
  let normalBox = win.document.getElementById("normalBox");
  let basicBox = win.document.getElementById("basicBox");
  is(normalBox.collapsed, true);
  is(basicBox.collapsed, false);
  dialog.cancelDialog();
  BrowserTestUtils.removeTab(tab);

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      ExemptDomainFileTypePairsFromFileTypeDownloadWarnings: [
        {
          file_extension: "jnlp",
          domains: ["example.com"],
        },
      ],
    },
  });

  windowObserver = BrowserTestUtils.domWindowOpenedAndLoaded();

  tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "example.jnlp",
    waitForLoad: false,
  });
  win = await windowObserver;

  dialog = win.document.querySelector("dialog");
  normalBox = win.document.getElementById("normalBox");
  basicBox = win.document.getElementById("basicBox");
  is(normalBox.collapsed, false);
  is(basicBox.collapsed, true);
  dialog.cancelDialog();
  BrowserTestUtils.removeTab(tab);

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {},
  });
});
