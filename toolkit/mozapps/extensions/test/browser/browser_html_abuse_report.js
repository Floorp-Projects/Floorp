/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_task(async function setup() {
  await AbuseReportTestUtils.setup();

  // Run abuse reports tests in the "about:addons sub-frame" mode.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.openDialog", false]],
  });
});

// Add all the test tasks shared with browser_html_abuse_report.js.
addCommonAbuseReportTestTasks();

/**
 * Test tasks specific to the abuse report opened as an about:addons subframe.
 */

// This test case verifies that:
// - the about:addons XUL page contains the addon-abuse-report-xulframe element,
//   and that
//   it is part of a XUL stack alongside with the rest of a about:addons
//   XUL page
// - the addon-abuse-report-xulframe contains a transparent browser XUL element
//   and it is shown and hidden as expected when its addon-id attribute is set
//   and then removed
// - the addon-abuse-report-xulframe move the focus to the abuse-report-panel
//   embedded into it,
// - it is automatically hidden when the abuse-report-panel has been closed
//   by the user, and the focus is returned back to the about:addons page.
add_task(async function addon_abusereport_xulframe() {
  const extension = await installTestExtension();

  const addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(addon, "The test add-on has been found");

  await openAboutAddons();
  const el = AbuseReportTestUtils.getReportFrame();

  // Verify that the abuse report XUL WebComponent is positioned in the
  // XUL about:addons page at the expected position.
  ok(el, "Got an addon-abuse-report-xulframe element in the about:addons page");
  is(el.parentNode.tagName, "stack", "Got the expected parent element");
  is(
    el.previousElementSibling.tagName,
    "hbox",
    "Got the expected previous sibling element"
  );
  is(
    el.parentNode.lastElementChild,
    el,
    "The addon-abuse-report-xulframe is the last element of the XUL stack"
  );
  ok(
    !el.hasAttribute("addon-id"),
    "The addon-id attribute should be initially empty"
  );

  // Set the addon-id attribute and check that the abuse report elements is
  // being shown.
  const onceUpdated = BrowserTestUtils.waitForEvent(el, "abuse-report:updated");
  el.openReport({ addonId: ADDON_ID, reportEntryPoint: "test" });

  // Wait the abuse report to be loaded.
  const abuseReportEl = await el.promiseAbuseReport;
  await onceUpdated;

  ok(!el.hidden, "The addon-abuse-report-xulframe element is visible");
  is(
    el.getAttribute("addon-id"),
    ADDON_ID,
    "Got the expected addon-id attribute set on the frame element"
  );
  is(
    el.getAttribute("report-entry-point"),
    "test",
    "Got the expected report-entry-point attribute set on the frame element"
  );

  const browser = el.querySelector("browser");

  is(
    gManagerWindow.document.activeElement,
    browser,
    "The addon-abuse-report-xulframe has been focused"
  );
  ok(browser, "The addon-abuse-report-xulframe contains a XUL browser element");
  is(
    browser.getAttribute("transparent"),
    "true",
    "The XUL browser element is transparent as expected"
  );

  ok(abuseReportEl, "Got an addon-abuse-report element");
  is(
    abuseReportEl.addonId,
    ADDON_ID,
    "The addon-abuse-report element has the expected addonId property"
  );
  ok(
    browser.contentDocument.contains(abuseReportEl),
    "The addon-abuse-report element is part of the embedded XUL browser"
  );

  await extension.unload();
  await closeAboutAddons();
});

// Test addon-abuse-xulframe auto hiding scenario.
add_task(async function addon_abusereport_xulframe_hiding() {
  const extension = await installTestExtension();

  const addon = await AddonManager.getAddonByID(ADDON_ID);
  const abuseReportEl = await AbuseReportTestUtils.openReport(extension.id);
  await AbuseReportTestUtils.promiseReportRendered(abuseReportEl);

  const el = AbuseReportTestUtils.getReportFrame();
  ok(!el.hidden, "The addon-abuse-report-xulframe element is visible");

  const browser = el.querySelector("browser");

  async function assertAbuseReportFrameHidden(actionFn, msg) {
    info(`Test ${msg}`);

    const panelEl = await AbuseReportTestUtils.openReport(addon.id);
    const frameEl = AbuseReportTestUtils.getReportFrame();

    ok(!frameEl.hidden, "The abuse report frame is visible");

    const onceFrameHidden = BrowserTestUtils.waitForEvent(
      frameEl,
      "abuse-report:frame-hidden"
    );
    await actionFn({ frameEl, panelEl });
    await onceFrameHidden;

    ok(
      !panelEl.hasAttribute("addon-id"),
      "addon-id attribute removed from the addon-abuse-report element"
    );

    ok(
      gManagerWindow.document.activeElement != browser,
      "addon-abuse-report-xulframe returned focus back to about:addons"
    );

    await closeAboutAddons();
  }

  const TESTS = [
    [
      async ({ panelEl }) => {
        panelEl.dispatchEvent(new CustomEvent("abuse-report:cancel"));
      },
      "addon report panel hidden on abuse-report:cancel event",
    ],
    [
      async () => {
        await EventUtils.synthesizeKey(
          "KEY_Escape",
          {},
          abuseReportEl.ownerGlobal
        );
      },
      "addon report panel hidden on Escape key pressed in the xulframe window",
    ],
    [
      async () => {
        await EventUtils.synthesizeKey("KEY_Escape", {}, gManagerWindow);
      },
      "addon report panel hidden on Escape key pressed about:addons window",
    ],
    [
      async ({ panelEl }) => {
        await EventUtils.synthesizeMouseAtCenter(
          panelEl._iconClose,
          {},
          panelEl.ownerGlobal
        );
      },
      "addon report panel hidden on close icon click",
    ],
    [
      async ({ panelEl }) => {
        await EventUtils.synthesizeMouseAtCenter(
          panelEl._btnCancel,
          {},
          panelEl.ownerGlobal
        );
      },
      "addon report panel hidden on close button click",
    ],
  ];

  for (const test of TESTS) {
    await assertAbuseReportFrameHidden(...test);
  }

  await extension.unload();
});

// This test case verifies that the expected addon metadata have been
// set in the abuse report panel, and they gets refreshed as expected
// when it is reused to report another extension.
add_task(async function test_abusereport_panel_refresh() {
  const EXT_ID1 = "test-panel-refresh@mochi.test";
  const EXT_ID2 = "test-panel-refresh-2@mochi.test";
  let addon;
  let extension;
  let reportPanel;

  async function getAbuseReportForManifest(addonId, manifest) {
    extension = await installTestExtension(addonId, "extension", manifest);

    addon = await AddonManager.getAddonByID(extension.id);
    ok(addon, "The test add-on has been found");

    const el = AbuseReportTestUtils.getReportFrame();

    const onceUpdated = BrowserTestUtils.waitForEvent(
      el,
      "abuse-report:updated"
    );
    AbuseReportTestUtils.triggerNewReport(addon.id, REPORT_ENTRY_POINT);
    const evt = await onceUpdated;
    is(
      evt.detail.addonId,
      addonId,
      `Abuse Report panel updated for ${addon.id}`
    );

    return el.promiseAbuseReport;
  }

  async function cancelAbuseReport() {
    const onceFrameHidden = BrowserTestUtils.waitForEvent(
      AbuseReportTestUtils.getReportFrame(),
      "abuse-report:frame-hidden"
    );
    reportPanel._btnCancel.click();
    await onceFrameHidden;
  }

  function assertExtensionMetadata(panel, expected) {
    let name = panel.querySelector(".addon-name").textContent;
    let authorLinkEl = reportPanel.querySelector("a.author");
    Assert.deepEqual(
      {
        name,
        author: authorLinkEl.textContent.trim(),
        homepage_url: authorLinkEl.getAttribute("href"),
        icon_url: panel.querySelector(".addon-icon").getAttribute("src"),
      },
      expected,
      "Got the expected addon metadata"
    );
  }

  await openAboutAddons();

  reportPanel = await getAbuseReportForManifest(EXT_ID1);
  let { name, author, homepage_url } = BASE_TEST_MANIFEST;

  assertExtensionMetadata(reportPanel, {
    name,
    author,
    homepage_url,
    icon_url: addon.iconURL,
  });

  await extension.unload();

  // Cancel the abuse report and then trigger it again on a second extension
  // to verify that the reused abuse report panel is refreshed accordingly.
  await cancelAbuseReport();

  const extData2 = {
    name: "Test extension 2",
    developer: {
      name: "The extension developer",
      url: "http://the.extension.url",
    },
  };
  const reportPanel2 = await getAbuseReportForManifest(EXT_ID2, extData2);
  is(reportPanel2, reportPanel, "Expect the abuse report panel to be reused");

  assertExtensionMetadata(reportPanel, {
    name: extData2.name,
    author: extData2.developer.name,
    homepage_url: extData2.developer.url,
    icon_url: addon.iconURL,
  });

  const allButtons = Array.from(reportPanel.querySelectorAll("button")).filter(
    el => el !== reportPanel._iconClose
  );
  ok(!!allButtons.length, "panel buttons should have been found");
  ok(
    allButtons.every(el => el.hasAttribute("data-l10n-id")),
    "All the panel buttons have a data-l10n-id"
  );

  await extension.unload();
  await closeAboutAddons();
});
