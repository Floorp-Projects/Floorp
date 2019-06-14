/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

const {
  AbuseReporter,
} = ChromeUtils.import("resource://gre/modules/AbuseReporter.jsm");
const {
  AddonTestUtils,
} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {
  ExtensionCommon,
} = ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");

const {makeWidgetId} = ExtensionCommon;

const ADDON_ID = "test-extension-to-report@mochi.test";
const REPORT_ENTRY_POINT = "menu";
const BASE_TEST_MANIFEST = {
  name: "Fake extension to report",
  author: "Fake author",
  homepage_url: "https://fake.extension.url/",
  applications: {gecko: {id: ADDON_ID}},
  icons: {
    32: "test-icon.png",
  },
};
const DEFAULT_BUILTIN_THEME_ID = "default-theme@mozilla.org";
const EXT_DICTIONARY_ADDON_ID = "fake-dictionary@mochi.test";
const EXT_LANGPACK_ADDON_ID = "fake-langpack@mochi.test";
const EXT_WITH_PRIVILEGED_URL_ID = "ext-with-privileged-url@mochi.test";
const EXT_SYSTEM_ADDON_ID = "test-system-addon@mochi.test";
const EXT_UNSUPPORTED_TYPE_ADDON_ID = "report-unsupported-type@mochi.test";
const THEME_NO_UNINSTALL_ID = "theme-without-perm-can-uninstall@mochi.test";

let gProvider;
let gHtmlAboutAddonsWindow;
let gManagerWindow;
let apiRequestHandler;

AddonTestUtils.initMochitest(this);

// Init test report api server.
const server = AddonTestUtils.createHttpServer({hosts: ["test.addons.org"]});
server.registerPathHandler("/api/report/", (request, response) => {
  const stream = request.bodyInputStream;
  const buffer = NetUtil.readInputStream(stream, stream.available());
  const data = new TextDecoder().decode(buffer);
  apiRequestHandler({data, request, response});
});

function handleSubmitRequest({request, response}) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.write("{}");
}

function createPromptConfirmEx({
  remove = false, report = false, expectCheckboxHidden = false,
} = {}) {
  return (...args) => {
    const checkboxState = args.pop();
    const checkboxMessage = args.pop();
    is(checkboxState && checkboxState.value, false,
       "checkboxState should be initially false");
    if (expectCheckboxHidden) {
      ok(!checkboxMessage,
         "Should not have a checkboxMessage in promptService.confirmEx call");
    } else {
      ok(checkboxMessage,
         "Got a checkboxMessage in promptService.confirmEx call");
    }

    // Report checkbox selected.
    checkboxState.value = report;

    // Remove accepted.
    return remove ? 0 : 1;
  };
}

async function openAboutAddons(type = "extension") {
  const win = await loadInitialView(type);
  gHtmlAboutAddonsWindow = win;
  gManagerWindow = win.managerWindow;
}

async function closeAboutAddons() {
  if (gHtmlAboutAddonsWindow) {
    await closeView(gHtmlAboutAddonsWindow);
    gHtmlAboutAddonsWindow = null;
    gManagerWindow = null;
  }
}

async function assertReportActionHidden(gManagerWindow, extId) {
  await gManagerWindow.htmlBrowserLoaded;
  const {contentDocument: doc} = gManagerWindow.getHtmlBrowser();

  let addonCard = doc.querySelector(
    `addon-list addon-card[addon-id="${extId}"]`);
  ok(addonCard, `Got the addon-card for the ${extId} test extension`);

  let reportButton = addonCard.querySelector("[action=report]");
  ok(reportButton, `Got the report action for ${extId}`);
  ok(reportButton.hidden, `${extId} report action should be hidden`);
}

async function installTestExtension(
  id = ADDON_ID, type = "extension", manifest = {}
) {
  const additionalProps = type === "theme" ? {
    theme: {
      colors: {
        frame: "#a14040",
        tab_background_text: "#fac96e",
      },
    },
  } : {};
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      ...BASE_TEST_MANIFEST,
      ...additionalProps,
      ...manifest,
      applications: {gecko: {id}},
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  return extension;
}

function getAbuseReportFrame() {
  return gManagerWindow.document.querySelector(
    "addon-abuse-report-xulframe");
}

function getAbuseReasons(abuseReportEl) {
  return Object.keys(abuseReportEl.ownerGlobal.ABUSE_REPORT_REASONS);
}

function getAbuseReasonInfo(abuseReportEl, reason) {
  return abuseReportEl.ownerGlobal.ABUSE_REPORT_REASONS[reason];
}

function triggerNewAbuseReport(addonId, reportEntryPoint) {
  const el = getAbuseReportFrame();
  el.ownerGlobal.openAbuseReport({addonId, reportEntryPoint});
}

function triggerSubmitAbuseReport(reason, message) {
  const el = getAbuseReportFrame();
  el.handleEvent(new CustomEvent("abuse-report:submit", {
    detail: {report: el.report, reason, message},
  }));
}

async function openAbuseReport(addonId, reportEntryPoint = REPORT_ENTRY_POINT) {
  // Close the current about:addons window if it has been leaved open from
  // a previous test case failure.
  if (gHtmlAboutAddonsWindow) {
    await closeAboutAddons();
  }

  await openAboutAddons();
  const el = getAbuseReportFrame();

  const onceUpdated = BrowserTestUtils.waitForEvent(el, "abuse-report:updated");

  triggerNewAbuseReport(addonId, reportEntryPoint);

  // The abuse report panel is collecting the report metadata and rendering
  // asynchronously, await on the "abuse-report:updated" event to be sure that
  // it has been rendered.
  const evt = await onceUpdated;
  is(evt.detail.addonId, addonId, `Abuse Report panel updated for ${addonId}`);

  return el.promiseAbuseReport;
}

async function promiseAbuseReportRendered(abuseReportEl) {
  let el = abuseReportEl;
  if (!el) {
    const frame = getAbuseReportFrame();
    el = await frame.promiseAbuseReport;
  }
  return el._radioCheckedReason ? Promise.resolve() :
    BrowserTestUtils.waitForEvent(el, "abuse-report:updated",
                                  "Wait the abuse report panel to be rendered");
}

async function promiseAbuseReportUpdated(abuseReportEl, panel) {
  const evt = await BrowserTestUtils.waitForEvent(
    abuseReportEl, "abuse-report:updated",
    "Wait abuse report panel update");

  if (panel) {
    is(evt.detail.panel, panel, `Got a "${panel}" update event`);
    switch (evt.detail.panel) {
      case "reasons":
        ok(!abuseReportEl._reasonsPanel.hidden,
           "Reasons panel should be visible");
        ok(abuseReportEl._submitPanel.hidden,
           "Submit panel should be hidden");
        break;
      case "submit":
        ok(abuseReportEl._reasonsPanel.hidden,
           "Reasons panel should be hidden");
        ok(!abuseReportEl._submitPanel.hidden,
           "Submit panel should be visible");
        break;
    }
  }
}

function promiseMessageBars(expectedMessageBarCount) {
  return new Promise(resolve => {
    const details = [];
    function listener(evt) {
      details.push(evt.detail);
      if (details.length >= expectedMessageBarCount) {
        cleanup();
        resolve(details);
      }
    }
    function cleanup() {
      if (gHtmlAboutAddonsWindow) {
        gHtmlAboutAddonsWindow.document.removeEventListener(
          "abuse-report:new-message-bar", listener);
      }
    }
    gHtmlAboutAddonsWindow.document.addEventListener(
      "abuse-report:new-message-bar", listener);
  });
}

add_task(async function setup() {
  // Enable html about:addons and the abuse reporting.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", true],
      ["extensions.abuseReport.enabled", true],
      ["extensions.abuseReport.url", "http://test.addons.org/api/report/"],
    ],
  });

  gProvider = new MockProvider();
  gProvider.createAddons([{
    id: THEME_NO_UNINSTALL_ID,
    name: "This theme cannot be uninstalled",
    version: "1.1",
    creator: {name: "Theme creator", url: "http://example.com/creator"},
    type: "theme",
    permissions: 0,
  }, {
    id: EXT_WITH_PRIVILEGED_URL_ID,
    name: "This extension has an unexpected privileged creator URL",
    version: "1.1",
    creator: {name: "creator", url: "about:config"},
    type: "extension",
  }, {
    id: EXT_SYSTEM_ADDON_ID,
    name: "This is a system addon",
    version: "1.1",
    creator: {name: "creator", url: "http://example.com/creator"},
    type: "extension",
    isSystem: true,
  }, {
    id: EXT_UNSUPPORTED_TYPE_ADDON_ID,
    name: "This is a fake unsupported addon type",
    version: "1.1",
    type: "unsupported_addon_type",
  }, {
    id: EXT_LANGPACK_ADDON_ID,
    name: "This is a fake langpack",
    version: "1.1",
    type: "locale",
  }, {
    id: EXT_DICTIONARY_ADDON_ID,
    name: "This is a fake dictionary",
    version: "1.1",
    type: "dictionary",
  }]);
});

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
  const el = getAbuseReportFrame();

  // Verify that the abuse report XUL WebComponent is positioned in the
  // XUL about:addons page at the expected position.
  ok(el, "Got an addon-abuse-report-xulframe element in the about:addons page");
  is(el.parentNode.tagName, "stack", "Got the expected parent element");
  is(el.previousElementSibling.tagName, "hbox",
     "Got the expected previous sibling element");
  is(el.parentNode.lastElementChild, el,
     "The addon-abuse-report-xulframe is the last element of the XUL stack");
  ok(!el.hasAttribute("addon-id"),
     "The addon-id attribute should be initially empty");

  // Set the addon-id attribute and check that the abuse report elements is
  // being shown.
  const onceUpdated = BrowserTestUtils.waitForEvent(el, "abuse-report:updated");
  el.openReport({addonId: ADDON_ID, reportEntryPoint: "test"});

  // Wait the abuse report to be loaded.
  const abuseReportEl = await el.promiseAbuseReport;
  await onceUpdated;

  ok(!el.hidden, "The addon-abuse-report-xulframe element is visible");
  is(el.getAttribute("addon-id"), ADDON_ID,
     "Got the expected addon-id attribute set on the frame element");
  is(el.getAttribute("report-entry-point"), "test",
     "Got the expected report-entry-point attribute set on the frame element");

  const browser = el.querySelector("browser");

  is(gManagerWindow.document.activeElement, browser,
     "The addon-abuse-report-xulframe has been focused");
  ok(browser, "The addon-abuse-report-xulframe contains a XUL browser element");
  is(browser.getAttribute("transparent"), "true",
     "The XUL browser element is transparent as expected");

  ok(abuseReportEl, "Got an addon-abuse-report element");
  is(abuseReportEl.addonId, ADDON_ID,
     "The addon-abuse-report element has the expected addonId property");
  ok(browser.contentDocument.contains(abuseReportEl),
     "The addon-abuse-report element is part of the embedded XUL browser");

  await extension.unload();
  await closeAboutAddons();
});

// Test addon-abuse-xulframe auto hiding scenario.
add_task(async function addon_abusereport_xulframe_hiding() {
  const extension = await installTestExtension();

  const addon = await AddonManager.getAddonByID(ADDON_ID);
  const abuseReportEl = await openAbuseReport(extension.id);
  await promiseAbuseReportRendered(abuseReportEl);

  const el = getAbuseReportFrame();
  ok(!el.hidden, "The addon-abuse-report-xulframe element is visible");

  const browser = el.querySelector("browser");

  async function assertAbuseReportFrameHidden(actionFn, msg) {
    info(`Test ${msg}`);

    const panelEl = await openAbuseReport(addon.id);
    const frameEl = getAbuseReportFrame();

    ok(!frameEl.hidden, "The abuse report frame is visible");

    const onceFrameHidden = BrowserTestUtils.waitForEvent(
      frameEl, "abuse-report:frame-hidden");
    await actionFn({frameEl, panelEl});
    await onceFrameHidden;

    ok(!panelEl.hasAttribute("addon-id"),
       "addon-id attribute removed from the addon-abuse-report element");

    ok(gManagerWindow.document.activeElement != browser,
       "addon-abuse-report-xulframe returned focus back to about:addons");

    await closeAboutAddons();
  }

  const TESTS = [
    [
      async ({panelEl}) => {
        panelEl.dispatchEvent(new CustomEvent("abuse-report:cancel"));
      },
      "addon report panel hidden on abuse-report:cancel event",
    ],
    [
      async () => {
        await EventUtils.synthesizeKey("KEY_Escape", {},
                                       abuseReportEl.ownerGlobal);
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
      async ({panelEl}) => {
        await EventUtils.synthesizeMouseAtCenter(panelEl._iconClose, {},
                                                 panelEl.ownerGlobal);
      },
      "addon report panel hidden on close icon click",
    ],
    [
      async ({panelEl}) => {
        await EventUtils.synthesizeMouseAtCenter(panelEl._btnCancel, {},
                                                 panelEl.ownerGlobal);
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

  async function getAbuseReportForManifest(addonId, manifest) {
    extension = await installTestExtension(addonId, "extension", manifest);

    addon = await AddonManager.getAddonByID(extension.id);
    ok(addon, "The test add-on has been found");

    await addon.uninstall(true);

    return openAbuseReport(extension.id);
  }

  function assertExtensionMetadata(panel, expected) {
    let name = panel.querySelector(".addon-name").textContent;
    let authorLinkEl = reportPanel.querySelector("a.author");
    Assert.deepEqual({
      name,
      author: authorLinkEl.textContent.trim(),
      homepage_url: authorLinkEl.getAttribute("href"),
      icon_url: panel.querySelector(".addon-icon").getAttribute("src"),
    }, expected, "Got the expected addon metadata");
  }

  let reportPanel = await getAbuseReportForManifest(EXT_ID1);
  let {name, author, homepage_url} = BASE_TEST_MANIFEST;

  assertExtensionMetadata(reportPanel, {
    name, author, homepage_url,
    icon_url: addon.iconURL,
  });

  await addon.cancelUninstall();
  await extension.unload();
  await closeAboutAddons();

  const extData2 = {
    name: "Test extension 2",
    developer: {
      name: "The extension developer",
      url: "http://the.extension.url",
    },
  };
  reportPanel = await getAbuseReportForManifest(EXT_ID2, extData2);

  assertExtensionMetadata(reportPanel, {
    name: extData2.name,
    author: extData2.developer.name,
    homepage_url: extData2.developer.url,
    icon_url: addon.iconURL,
  });

  const allButtons = Array.from(reportPanel.querySelectorAll("buttons"));
  ok(allButtons.every(el => el.hasAttribute("data-l10n-id")),
     "All the panel buttons have a data-l10n-id");

  await addon.cancelUninstall();
  await extension.unload();
  await closeAboutAddons();
});

// This test case verified that the abuse report panels contains a radio
// button for all the expected "abuse report reasons", they are grouped
// together under the same form field named "reason".
add_task(async function test_abusereport_issuelist() {
  const extension = await installTestExtension();

  const abuseReportEl = await openAbuseReport(extension.id);
  await promiseAbuseReportRendered(abuseReportEl);

  const reasonsPanel = abuseReportEl._reasonsPanel;
  const radioButtons = reasonsPanel.querySelectorAll("[type=radio]");
  const selectedRadios = reasonsPanel.querySelectorAll("[type=radio]:checked");

  is(selectedRadios.length, 1, "Expect only one radio button selected");
  is(selectedRadios[0], radioButtons[0],
     "Expect the first radio button to be selected");

  is(abuseReportEl.reason, radioButtons[0].value,
     `The reason property has the expected value: ${radioButtons[0].value}`);

  const reasons = Array.from(radioButtons).map(el => el.value);
  Assert.deepEqual(reasons.sort(), getAbuseReasons(abuseReportEl).sort(),
                   `Got a radio button for the expected reasons`);

  for (const radio of radioButtons) {
    const reasonInfo = getAbuseReasonInfo(abuseReportEl, radio.value);
    const expectExampleHidden = reasonInfo &&
                                reasonInfo.isExampleHidden("extension");
    is(radio.parentNode.querySelector(".reason-example").hidden,
       expectExampleHidden,
       `Got expected visibility on the example for reason "${radio.value}"`);
  }

  info("Change the selected reason to " + radioButtons[3].value);
  radioButtons[3].checked = true;
  is(abuseReportEl.reason, radioButtons[3].value,
     "The reason property has the expected value");

  await extension.unload();
  await closeAboutAddons();
});

// This test case verifies that the abuse report panel:
// - switches from its "reasons list" mode to its "submit report" mode when the
//   "next" button is clicked
// - goes back to the "reasons list" mode when the "go back" button is clicked
// - the abuse report panel is closed when the "close" icon is clicked
add_task(async function test_abusereport_submitpanel() {
  const extension = await installTestExtension();

  const abuseReportEl = await openAbuseReport(extension.id);
  await promiseAbuseReportRendered(abuseReportEl);

  ok(!abuseReportEl._reasonsPanel.hidden,
     "The list of abuse reasons is the currently visible");
  ok(abuseReportEl._submitPanel.hidden,
    "The submit panel is the currently hidden");

  info("Clicking the 'next' button");
  let onceUpdated = promiseAbuseReportUpdated(abuseReportEl, "submit");
  EventUtils.synthesizeMouseAtCenter(abuseReportEl._btnNext, {},
    abuseReportEl.ownerGlobal);
  await onceUpdated;

  info("Clicking the 'go back' button");
  onceUpdated = promiseAbuseReportUpdated(abuseReportEl, "reasons");

  EventUtils.synthesizeMouseAtCenter(abuseReportEl._btnGoBack, {},
                                     abuseReportEl.ownerGlobal);
  await onceUpdated;

  info("Clicking the 'close' icon");
  const onceCancelEvent = BrowserTestUtils.waitForEvent(abuseReportEl,
                                                        "abuse-report:cancel");
  EventUtils.synthesizeMouseAtCenter(abuseReportEl._iconClose, {},
                                     abuseReportEl.ownerGlobal);
  await onceCancelEvent;

  const frameEl = getAbuseReportFrame();

  ok(frameEl.hidden, "The abuse report frame has been hidden");
  ok(!frameEl.hasAttribute("addon-id"), "addon-id attribute has been removed");

  await extension.unload();
  await closeAboutAddons();
});

// This test case verifies that the abuse report panel sends the expected data
// in the "abuse-report:submit" event detail.
add_task(async function test_abusereport_submit() {
  // Reset the timestamp of the last report between tests.
  AbuseReporter._lastReportTimestamp = null;
  const extension = await installTestExtension();

  const abuseReportEl = await openAbuseReport(extension.id);
  await promiseAbuseReportRendered(abuseReportEl);

  ok(!abuseReportEl._reasonsPanel.hidden,
     "The list of abuse reasons is the currently visible");

  info("Clicking the 'next' button");
  let onceUpdated = promiseAbuseReportUpdated(abuseReportEl, "submit");
  EventUtils.synthesizeMouseAtCenter(abuseReportEl._btnNext, {},
                                     abuseReportEl.ownerGlobal);
  await onceUpdated;

  is(abuseReportEl.message, "", "The abuse report message is initially empty");

  info("Test typing a message in the abuse report submit panel textarea");
  const typedMessage = "Description of the extension abuse report";
  await EventUtils.synthesizeCompositionChange({
    composition: {
      string: typedMessage,
      clauses: [{
        length: typedMessage.length,
        attr: Ci.nsITextInputProcessor.ATTR_RAW_CLAUSE,
      }],
    },
  });

  is(abuseReportEl.message, typedMessage,
     "Got the expected typed message in the abuse report");

  const expectedDetail = {
    addonId: extension.id,
    reason: abuseReportEl.reason,
    message: abuseReportEl.message,
  };

  let reportSubmitted;
  const onReportSubmitted = new Promise(resolve => {
    apiRequestHandler = ({data, request, response}) => {
      reportSubmitted = JSON.parse(data);
      handleSubmitRequest({request, response});
      resolve();
    };
  });

  info("Clicking the 'submit' button");
  const onMessageBarsCreated = promiseMessageBars(2);

  const onceSubmitEvent = BrowserTestUtils.waitForEvent(abuseReportEl,
                                                        "abuse-report:submit");
  EventUtils.synthesizeMouseAtCenter(abuseReportEl._btnSubmit, {},
                                     abuseReportEl.ownerGlobal);
  const submitEvent = await onceSubmitEvent;

  const actualDetail = {
    addonId: submitEvent.detail.addonId,
    reason: submitEvent.detail.reason,
    message: submitEvent.detail.message,
  };
  Assert.deepEqual(
    actualDetail, expectedDetail,
    "Got the expected detail in the abuse-report:submit event");

  ok(submitEvent.detail.report,
     "Got a report object in the abuse-report:submit event detail");

  const frameEl = getAbuseReportFrame();

  info("Wait the report to be submitted to the api server");
  await onReportSubmitted;

  // Verify that, when the "abuse-report:submit" has been sent, the abuse report
  // panel has been hidden, the report has been submitted and the expected
  // message bar is created in the HTMl about addons page.
  ok(frameEl.hidden, "abuse report frame should be hidden");
  ok(!frameEl.hasAttribute("addon-id"),
     "addon-id attribute has been removed from the abuse report frame");
  ok(abuseReportEl.hidden, "abuse report panel should be hidden");

  is(reportSubmitted.addon, ADDON_ID,
     "Got the expected addon in the submitted report");
  is(reportSubmitted.reason, expectedDetail.reason,
     "Got the expected reason in the submitted report");
  is(reportSubmitted.message, expectedDetail.message,
     "Got the expected message in the submitted report");
  is(reportSubmitted.report_entry_point, REPORT_ENTRY_POINT,
     "Got the expected report_entry_point in the submitted report");

  info("Waiting the expected message bars to be created");
  const barDetails = await onMessageBarsCreated;
  is(barDetails.length, 2, "Expect two message bars to have been created");
  is(barDetails[0].definitionId, "submitting",
     "Got a submitting message bar as expected");
  is(barDetails[1].definitionId, "submitted",
     "Got a submitted message bar as expected");

  await extension.unload();
  await closeAboutAddons();
});

// This test case verifies that the abuse report panel contains the expected
// suggestions when the selected reason requires it (and urls are being set
// on the links elements included in the suggestions when expected).
async function test_abuse_report_suggestions(addonId) {
  const addon = await AddonManager.getAddonByID(addonId);

  const abuseReportEl = await openAbuseReport(addonId);
  await promiseAbuseReportRendered(abuseReportEl);

  const {
    _btnNext,
    _btnGoBack,
    _reasonsPanel,
    _submitPanel,
    _submitPanel: {
      _suggestions,
    },
  } = abuseReportEl;

  for (const reason of getAbuseReasons(abuseReportEl)) {
    const reasonInfo = getAbuseReasonInfo(abuseReportEl, reason);

    if (reasonInfo.isReasonHidden(addon.type)) {
      continue;
    }

    info(`Test suggestions for abuse reason "${reason}"`);

    // Select a reason with suggestions.
    let radioEl = abuseReportEl.querySelector(`#abuse-reason-${reason}`);
    ok(radioEl, `Found radio button for "${reason}"`);
    radioEl.checked = true;

    info("Clicking the 'next' button");
    let oncePanelUpdated = promiseAbuseReportUpdated(abuseReportEl, "submit");
    EventUtils.synthesizeMouseAtCenter(_btnNext, {}, _btnNext.ownerGlobal);
    await oncePanelUpdated;

    const localizedSuggestionsContent = Array.from(
      _suggestions.querySelectorAll("[data-l10n-id]")
    ).filter(el => !el.hidden);

    is(!_suggestions.hidden, !!reasonInfo.hasSuggestions,
       `Suggestions block has the expected visibility for "${reason}"`);
    if (reasonInfo.hasSuggestions) {
      ok(localizedSuggestionsContent.length > 0,
         `Category suggestions should not be empty for "${reason}"`);
    } else {
      ok(localizedSuggestionsContent.length === 0,
         `Category suggestions should be empty for "${reason}"`);
    }

    const extSupportLink = _suggestions.querySelector(
      ".extension-support-link");
    if (extSupportLink) {
      is(extSupportLink.getAttribute("href"), BASE_TEST_MANIFEST.homepage_url,
         "Got the expected extension-support-url");
    }

    const learnMoreLinks = [];
    for (const linkClass of _suggestions.LEARNMORE_LINKS) {
      learnMoreLinks.push(..._suggestions.querySelectorAll(linkClass));
    }

    if (learnMoreLinks.length > 0) {
      ok(learnMoreLinks.every(el => el.getAttribute("target") === "_blank"),
         "All the learn more links have target _blank");
      ok(learnMoreLinks.every(el => el.hasAttribute("href")),
         "All the learn more links have a url set");
    }

    info("Clicking the 'go back' button");
    oncePanelUpdated = promiseAbuseReportUpdated(abuseReportEl, "reasons");
    EventUtils.synthesizeMouseAtCenter(
      _btnGoBack, {}, abuseReportEl.ownerGlobal);
    await oncePanelUpdated;
    ok(!_reasonsPanel.hidden, "Reasons panel should be visible");
    ok(_submitPanel.hidden, "Submit panel should be hidden");
  }

  await closeAboutAddons();
}

add_task(async function test_abuse_report_suggestions_extension() {
  const EXT_ID = "test-extension-suggestions@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  await test_abuse_report_suggestions(EXT_ID);
  await extension.unload();
});

add_task(async function test_abuse_report_suggestions_theme() {
  const THEME_ID = "theme@mochi.test";
  const theme = await installTestExtension(THEME_ID, "theme");
  await test_abuse_report_suggestions(THEME_ID);
  await theme.unload();
});

// This test case verifies the message bars created on other
// scenarios (e.g. report creation and submissions errors).
add_task(async function test_abuse_report_message_bars() {
  const EXT_ID = "test-extension-report@mochi.test";
  const EXT_ID2 = "test-extension-report-2@mochi.test";
  const THEME_ID = "test-theme-report@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  const extension2 = await installTestExtension(EXT_ID2);
  const theme = await installTestExtension(THEME_ID, "theme");

  async function assertMessageBars(
    expectedMessageBarIds, testSetup, testMessageBarDetails
  ) {
    await openAboutAddons();
    const expectedLength = expectedMessageBarIds.length;
    const onMessageBarsCreated = promiseMessageBars(expectedLength);
    // Reset the timestamp of the last report between tests.
    AbuseReporter._lastReportTimestamp = null;
    await testSetup();
    info(`Waiting for ${expectedLength} message-bars to be created`);
    const barDetails = await onMessageBarsCreated;
    Assert.deepEqual(barDetails.map(d => d.definitionId), expectedMessageBarIds,
                     "Got the expected message bars");
    if (testMessageBarDetails) {
      await testMessageBarDetails(barDetails);
    }
    await closeAboutAddons();
  }

  function setTestRequestHandler(responseStatus, responseData) {
    apiRequestHandler = ({request, response}) => {
      response.setStatusLine(request.httpVersion, responseStatus, "Error");
      response.write(responseData);
    };
  }

  await assertMessageBars(["ERROR_ADDON_NOTFOUND"], async () => {
    info("Test message bars on addon not found");
    triggerNewAbuseReport(
      "non-existend-addon-id@mochi.test", REPORT_ENTRY_POINT);
  });

  await assertMessageBars(["submitting", "ERROR_RECENT_SUBMIT"], async () => {
    info("Test message bars on recent submission");
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    AbuseReporter.updateLastReportTimestamp();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_ABORTED_SUBMIT"], async () => {
    info("Test message bars on aborted submission");
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    const {report}  = getAbuseReportFrame();
    report.abort();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_SERVER"], async () => {
    info("Test message bars on server error");
    setTestRequestHandler(500);
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_CLIENT"], async () => {
    info("Test message bars on client error");
    setTestRequestHandler(400);
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_UNKNOWN"], async () => {
    info("Test message bars on unexpected status code");
    setTestRequestHandler(604);
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_UNKNOWN"], async () => {
    info("Test message bars on invalid json in the response data");
    setTestRequestHandler(200, "");
    triggerNewAbuseReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  // Verify message bar on add-on without perm_can_uninstall.
  await assertMessageBars([
    "submitting", "submitted-no-remove-action",
  ], async () => {
    info("Test message bars on report submitted on an addon without remove");
    setTestRequestHandler(200, "{}");
    triggerNewAbuseReport(THEME_NO_UNINSTALL_ID, "menu");
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  // Verify the 3 expected entry points:
  //   menu, toolbar_context_menu and uninstall
  // (See https://addons-server.readthedocs.io/en/latest/topics/api/abuse.html).
  await assertMessageBars(["submitting", "submitted"], async () => {
    info("Test message bars on report opened from addon options menu");
    setTestRequestHandler(200, "{}");
    triggerNewAbuseReport(EXT_ID, "menu");
    await promiseAbuseReportRendered();
    triggerSubmitAbuseReport("fake-reason", "fake-message");
  });

  for (const extId of [EXT_ID, THEME_ID]) {
    await assertMessageBars(["submitting", "submitted"], async () => {
      info(`Test message bars on ${extId} reported from toolbar contextmenu`);
      setTestRequestHandler(200, "{}");
      triggerNewAbuseReport(extId, "toolbar_context_menu");
      await promiseAbuseReportRendered();
      triggerSubmitAbuseReport("fake-reason", "fake-message");
    }, ([submittingDetails, submittedDetails]) => {
      const buttonsL10nId = Array.from(
        submittedDetails.messagebar.querySelectorAll("button")
      ).map(el => el.getAttribute("data-l10n-id"));
      if (extId === THEME_ID) {
        ok(buttonsL10nId.every(id => id.endsWith("-theme")),
           "submitted bar actions should use the Fluent id for themes");
      } else {
        ok(buttonsL10nId.every(id => id.endsWith("-extension")),
           "submitted bar actions should use the Fluent id for extensions");
      }
    });
  }

  for (const extId of [EXT_ID2, THEME_ID]) {
    const testFn = async () => {
      info(`Test message bars on ${extId} reported opened from addon removal`);
      setTestRequestHandler(200, "{}");
      triggerNewAbuseReport(extId, "uninstall");
      await promiseAbuseReportRendered();
      triggerSubmitAbuseReport("fake-reason", "fake-message");
    };
    await assertMessageBars(["submitting", "submitted-and-removed"], testFn);
  }

  await extension.unload();
  await extension2.unload();
  await theme.unload();
});

add_task(async function test_trigger_abusereport_from_aboutaddons_menu() {
  const EXT_ID = "test-report-from-aboutaddons-menu@mochi.test";
  const extension = await installTestExtension(EXT_ID);

  await openAboutAddons();
  await gManagerWindow.htmlBrowserLoaded;

  const abuseReportFrameEl = getAbuseReportFrame();
  ok(abuseReportFrameEl.hidden,
     "Abuse Report frame should be initially hidden");

  const {contentDocument: doc} = gManagerWindow.getHtmlBrowser();

  const addonCard = doc.querySelector(
    `addon-list addon-card[addon-id="${extension.id}"]`);
  ok(addonCard, "Got the addon-card for the test extension");

  const reportButton = addonCard.querySelector("[action=report]");
  ok(reportButton, "Got the report action for the test extension");

  const onceReportNew = BrowserTestUtils.waitForEvent(
    abuseReportFrameEl, "abuse-report:new");
  const onceReportFrameShown = BrowserTestUtils.waitForEvent(
    abuseReportFrameEl, "abuse-report:frame-shown");

  info("Click the report action and wait for the 'abuse-report:new' event");
  reportButton.click();
  const newReportEvent = await onceReportNew;

  Assert.deepEqual(newReportEvent.detail, {
    addonId: extension.id,
    reportEntryPoint: "menu",
  }, "Got the expected details in the 'abuse-report:new' event");

  info("Wait for the abuse report frame to be visible");
  await onceReportFrameShown;
  ok(!abuseReportFrameEl.hidden,
    "Abuse Report frame should be visible");

  info("Wait for the abuse report panel to be rendered");
  const abuseReportEl = await abuseReportFrameEl.promiseAbuseReport;
  await promiseAbuseReportRendered(abuseReportEl);

  is(abuseReportEl.addon && abuseReportEl.addon.id, extension.id,
     "Abuse Report panel rendered for the expected addonId");

  await closeAboutAddons();
  await extension.unload();
});

add_task(async function test_trigger_abusereport_from_aboutaddons_remove() {
  const EXT_ID = "test-report-from-aboutaddons-remove@mochi.test";

  // Test on a theme addon to cover the report checkbox included in the
  // uninstall dialog also on a theme.
  const extension = await installTestExtension(EXT_ID, "theme");

  await openAboutAddons("theme");
  await gManagerWindow.htmlBrowserLoaded;

  const abuseReportFrameEl = getAbuseReportFrame();
  ok(abuseReportFrameEl.hidden,
     "Abuse Report frame should be initially hidden");

  const {contentDocument: doc} = gManagerWindow.getHtmlBrowser();

  const addonCard = doc.querySelector(
    `addon-list addon-card[addon-id="${extension.id}"]`);
  ok(addonCard, "Got the addon-card for the test theme extension");

  const removeButton = addonCard.querySelector("[action=remove]");
  ok(removeButton, "Got the remove action for the test theme extension");

  const onceReportNew = BrowserTestUtils.waitForEvent(
    abuseReportFrameEl, "abuse-report:new");
  const onceReportFrameShown = BrowserTestUtils.waitForEvent(
    abuseReportFrameEl, "abuse-report:frame-shown");

  // Prepare the mocked prompt service.
  const promptService = mockPromptService();
  promptService.confirmEx = createPromptConfirmEx({remove: true, report: true});

  info("Click the report action and wait for the 'abuse-report:new' event");
  removeButton.click();
  const newReportEvent = await onceReportNew;

  Assert.deepEqual(newReportEvent.detail, {
    addonId: extension.id,
    reportEntryPoint: "uninstall",
  }, "Got the expected details in the 'abuse-report:new' event");

  info("Wait for the abuse report frame to be visible");
  await onceReportFrameShown;
  ok(!abuseReportFrameEl.hidden,
    "Abuse Report frame should be visible");

  info("Wait for the abuse report panel to be rendered");
  const abuseReportEl = await abuseReportFrameEl.promiseAbuseReport;
  await promiseAbuseReportRendered(abuseReportEl);

  is(abuseReportEl.addon && abuseReportEl.addon.id, extension.id,
     "Abuse Report panel rendered for the expected addonId");

  await closeAboutAddons();
  await extension.unload();
});

add_task(async function test_trigger_abusereport_from_browserAction_remove() {
  const EXT_ID = "test-report-from-browseraction-remove@mochi.test";
  const extension = await installTestExtension(EXT_ID, "extension", {
    browser_action: {},
  });

  // Prepare the mocked prompt service.
  const promptService = mockPromptService();
  promptService.confirmEx = createPromptConfirmEx({remove: true, report: true});

  await BrowserTestUtils.withNewTab("about:blank", async function() {
    info(`Open browserAction context menu in toolbar context menu`);
    let buttonId = `${makeWidgetId(EXT_ID)}-browser-action`;
    const menu = document.getElementById("toolbar-context-menu");
    const node = document.getElementById(CSS.escape(buttonId));
    const shown = BrowserTestUtils.waitForEvent(menu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(node, {type: "contextmenu"});
    await shown;

    info(`Clicking on "Remove Extension" context menu item`);
    let removeExtension = menu.querySelector(
      ".customize-context-removeExtension");
    removeExtension.click();

    // Wait about:addons to be loaded.
    const browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);
    is(browser.currentURI.spec, "about:addons",
      "about:addons tab currently selected");
    menu.hidePopup();

    const abuseReportFrame = browser.contentDocument
      .querySelector("addon-abuse-report-xulframe");

    ok(!abuseReportFrame.hidden,
      "Abuse report frame has the expected visibility");
    is(abuseReportFrame.report.addon.id, EXT_ID,
      "Abuse report frame has the expected addon id");
    is(abuseReportFrame.report.reportEntryPoint, "uninstall",
      "Abuse report frame has the expected reportEntryPoint");
  });

  await extension.unload();
});

// This test verify that the abuse report panel is opening the
// author link using a null triggeringPrincipal.
add_task(async function test_abuse_report_open_author_url() {
  const abuseReportEl = await openAbuseReport(EXT_WITH_PRIVILEGED_URL_ID);
  await promiseAbuseReportRendered(abuseReportEl);

  const authorLink = abuseReportEl._linkAddonAuthor;
  ok(authorLink, "Got the author link element");
  is(authorLink.href, "about:config",
     "Got a privileged url in the link element");

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [{
      // eslint-disable-next-line max-len
      message: /Security Error: Content at moz-nullprincipal:{.*} may not load or link to about:config/,
    }]);
  });

  let tabSwitched = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  authorLink.click();
  await tabSwitched;

  is(gBrowser.selectedBrowser.currentURI.spec, "about:blank",
     "Got about:blank loaded in the new tab");

  SimpleTest.endMonitorConsole();
  await waitForConsole;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_builtin_addons() {
  await openAboutAddons("theme");
  await assertReportActionHidden(gManagerWindow, DEFAULT_BUILTIN_THEME_ID);
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_system_addons() {
  await openAboutAddons("extension");
  await assertReportActionHidden(gManagerWindow, EXT_SYSTEM_ADDON_ID);
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_dictionary_addons() {
  await openAboutAddons("dictionary");
  await assertReportActionHidden(gManagerWindow, EXT_DICTIONARY_ADDON_ID);
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_langpack_addons() {
  await openAboutAddons("locale");
  await assertReportActionHidden(gManagerWindow, EXT_LANGPACK_ADDON_ID);
  await closeAboutAddons();
});

// This test verifies that triggering a report that would be immediately
// cancelled (e.g. because abuse reports for that extension type are not
// supported) the abuse report frame is being hidden as expected.
add_task(async function test_frame_hidden_on_report_unsupported_addontype() {
  await openAboutAddons();
  const el = getAbuseReportFrame();

  const onceCancelled = BrowserTestUtils.waitForEvent(
    el, "abuse-report:cancel");
  triggerNewAbuseReport(EXT_UNSUPPORTED_TYPE_ADDON_ID, "menu");

  await onceCancelled;

  is(el.hidden, true, `report frame hidden on automatically cancelled report`);

  await closeAboutAddons();
});

add_task(async function test_no_report_checkbox_for_unsupported_addon_types() {
  async function test_report_checkbox_hidden(addon) {
    await openAboutAddons(addon.type);
    await gManagerWindow.htmlBrowserLoaded;

    const abuseReportFrameEl = getAbuseReportFrame();
    ok(abuseReportFrameEl.hidden,
       "Abuse Report frame should be hidden");

    const {contentDocument: doc} = gManagerWindow.getHtmlBrowser();

    const addonCard = doc.querySelector(
      `addon-list addon-card[addon-id="${addon.id}"]`);
    ok(addonCard, "Got the addon-card for the test extension");

    const removeButton = addonCard.querySelector("[action=remove]");
    ok(removeButton, "Got the remove action for the test extension");

    // Prepare the mocked prompt service.
    const promptService = mockPromptService();
    promptService.confirmEx = createPromptConfirmEx({
      remove: true, report: false, expectCheckboxHidden: true,
    });

    info("Click the report action and wait for the addon to be removed");
    const promiseCardRemoved = BrowserTestUtils.waitForEvent(
      addonCard.closest("addon-list"), "remove");
    removeButton.click();
    await promiseCardRemoved;

    ok(abuseReportFrameEl.hidden,
      "Abuse Report frame should still be hidden");

    await closeAboutAddons();
  }

  const reportNotSupportedAddons = [{
    id: "fake-langpack-to-remove@mochi.test",
    name: "This is a fake langpack",
    version: "1.1",
    type: "locale",
  }, {
    id: "fake-dictionary-to-remove@mochi.test",
    name: "This is a fake dictionary",
    version: "1.1",
    type: "dictionary",
  }];

  gProvider.createAddons(reportNotSupportedAddons);

  for (const {id} of reportNotSupportedAddons) {
    const addon = await AddonManager.getAddonByID(id);
    await test_report_checkbox_hidden(addon);
  }
});
