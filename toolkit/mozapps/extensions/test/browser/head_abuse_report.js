/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

/* exported installTestExtension, addCommonAbuseReportTestTasks,
 *          handleSubmitRequest, waitForNewWindow, waitClosedWindow,
 *          AbuseReporter, AbuseReporterTestUtils, AddonTestUtils */

/* global mockPromptService, MockProvider, loadInitialView, closeView */

const { AbuseReporter } = ChromeUtils.import(
  "resource://gre/modules/AbuseReporter.jsm"
);
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);

const { makeWidgetId } = ExtensionCommon;

const ADDON_ID = "test-extension-to-report@mochi.test";
const REPORT_ENTRY_POINT = "menu";
const BASE_TEST_MANIFEST = {
  name: "Fake extension to report",
  author: "Fake author",
  homepage_url: "https://fake.extension.url/",
  applications: { gecko: { id: ADDON_ID } },
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

let gHtmlAboutAddonsWindow;
let gManagerWindow;

AddonTestUtils.initMochitest(this);

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

function waitForNewWindow() {
  return new Promise(resolve => {
    let listener = win => {
      Services.obs.removeObserver(listener, "toplevel-window-ready");
      resolve(win);
    };

    Services.obs.addObserver(listener, "toplevel-window-ready");
  });
}

function waitClosedWindow(win) {
  return new Promise((resolve, reject) => {
    function onWindowClosed() {
      if (win && !win.closed) {
        // If a specific window reference has been passed, then check
        // that the window is closed before resolving the promise.
        return;
      }
      Services.obs.removeObserver(onWindowClosed, "xul-window-destroyed");
      resolve();
    }
    Services.obs.addObserver(onWindowClosed, "xul-window-destroyed");
  });
}

async function installTestExtension(
  id = ADDON_ID,
  type = "extension",
  manifest = {}
) {
  const additionalProps =
    type === "theme"
      ? {
          theme: {
            colors: {
              frame: "#a14040",
              tab_background_text: "#fac96e",
            },
          },
        }
      : {};
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      ...BASE_TEST_MANIFEST,
      ...additionalProps,
      ...manifest,
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  return extension;
}

function handleSubmitRequest({ request, response }) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.write("{}");
}

const AbuseReportTestUtils = {
  _mockProvider: null,
  _mockServer: null,
  _abuseRequestHandlers: [],

  // Mock addon details API endpoint.
  amoAddonDetailsMap: new Map(),

  // Setup the test environment by setting the expected prefs and
  // initializing MockProvider and the mock AMO server.
  async setup() {
    // Enable html about:addons and the abuse reporting and
    // set the api endpoints url to the mock service.
    await SpecialPowers.pushPrefEnv({
      set: [
        ["extensions.abuseReport.enabled", true],
        ["extensions.abuseReport.url", "http://test.addons.org/api/report/"],
        [
          "extensions.abuseReport.amoDetailsURL",
          "http://test.addons.org/api/addons/addon/",
        ],
      ],
    });

    this._setupMockProvider();
    this._setupMockServer();
  },

  // Returns the addon-abuse-report-xulframe element from the currently open
  // about:addons tab.
  getReportFrame(managerWindow = gManagerWindow) {
    return managerWindow.document.querySelector("addon-abuse-report-xulframe");
  },

  // Returns the currently open abuse report dialog window (if any).
  getReportDialog() {
    return Services.ww.getWindowByName("addons-abuse-report-dialog", null);
  },

  // Returns the parameters related to the report dialog (if any).
  getReportDialogParams() {
    const win = this.getReportDialog();
    return win && win.arguments[0] && win.arguments[0].wrappedJSObject;
  },

  // Returns a reference to the addon-abuse-report element from the currently
  // open abuse report.
  getReportPanel() {
    if (AbuseReporter.openDialogDisabled) {
      const frame = this.getReportFrame();
      return frame
        .querySelector("browser")
        .contentDocument.querySelector("addon-abuse-report");
    }

    const win = this.getReportDialog();
    ok(win, "Got an abuse report dialog open");
    return win && win.document.querySelector("addon-abuse-report");
  },

  // Returns the list of abuse report reasons.
  getReasons(abuseReportEl) {
    return Object.keys(abuseReportEl.ownerGlobal.ABUSE_REPORT_REASONS);
  },

  // Returns the info related to a given abuse report reason.
  getReasonInfo(abuseReportEl, reason) {
    return abuseReportEl.ownerGlobal.ABUSE_REPORT_REASONS[reason];
  },

  async promiseReportOpened({ addonId, reportEntryPoint, managerWindow }) {
    let abuseReportEl;

    if (AbuseReporter.openDialogDisabled) {
      const win = managerWindow || gManagerWindow;
      ok(win, "Expect about:addons window to be found");
      const frame = AbuseReportTestUtils.getReportFrame(win);
      ok(frame, "Found an abuse report frame");

      if (frame.hidden) {
        const onceReportFrameShown = BrowserTestUtils.waitForEvent(
          frame,
          "abuse-report:frame-shown"
        );
        info("Wait for the abuse report frame to be visible");
        await onceReportFrameShown;
        ok(!frame.hidden, "Abuse Report frame should be visible");
      }
      info("Wait for the abuse report panel to be rendered");
      abuseReportEl = await frame.promiseAbuseReport;
      await AbuseReportTestUtils.promiseReportRendered(abuseReportEl);
    } else {
      if (!this.getReportDialog()) {
        info("Wait for the report dialog window");
        const dialog = await waitForNewWindow();
        is(dialog, this.getReportDialog(), "Report dialog opened");
      }
      info("Wait for the abuse report panel render");
      abuseReportEl = await AbuseReportTestUtils.promiseReportDialogRendered();
    }

    ok(abuseReportEl, "Got an abuse report panel");
    is(
      abuseReportEl.addon && abuseReportEl.addon.id,
      addonId,
      "Abuse Report panel rendered for the expected addonId"
    );
    is(
      abuseReportEl._report && abuseReportEl._report.reportEntryPoint,
      reportEntryPoint,
      "Abuse Report panel rendered for the expected reportEntryPoint"
    );

    return abuseReportEl;
  },

  // Return a promise resolved when the currently open report panel
  // is closed (for both the "about:addons sub-frame" and
  // "dialog window" modes).
  // Also asserts that a specific report panel element has been closed,
  // if one has been provided through the optional panel parameter.
  async promiseReportClosed(panel) {
    function assertPanelClosed() {
      // Assert that the panel has been closed (if the caller has passed it).
      if (panel) {
        if (AbuseReporter.openDialogDisabled) {
          ok(panel.hidden, "abuse report panel should be hidden");
        } else {
          ok(!panel.ownerGlobal, "abuse report dialog closed");
        }
      }
    }

    // Test helper implementation for "about:addons sub-frame" mode.
    if (AbuseReporter.openDialogDisabled) {
      const frame = panel
        ? panel.ownerGlobal.parent.document.querySelector(
            "addon-abuse-report-xulframe"
          )
        : this.getReportFrame();
      if (!frame || frame.hidden) {
        throw Error("Expected report frame not found or already hidden");
      }
      await BrowserTestUtils.waitForEvent(frame, "abuse-report:frame-hidden");
      ok(frame.hidden, "abuse report frame should be hidden");
      ok(
        !frame.hasAttribute("addon-id"),
        "addon-id attribute has been removed from the abuse report frame"
      );
      assertPanelClosed();
      return;
    }

    // Test helper implementation for "dialog window" mode.
    const win = panel ? panel.ownerGlobal : this.getReportDialog();
    if (!win || win.closed) {
      throw Error("Expected report dialog not found or already closed");
    }

    await waitClosedWindow(win);
    assertPanelClosed();
  },

  // Returns a promise resolved when the report panel has been rendered
  // (rejects is there is no dialog currently open).
  async promiseReportDialogRendered() {
    const params = this.getReportDialogParams();
    if (!params) {
      throw new Error("abuse report dialog not found");
    }
    return params.promiseReportPanel;
  },

  // Given a `requestHandler` function, an HTTP server handler function
  // to use to handle a report submit request received by the mock AMO server),
  // returns a promise resolved when the mock AMO server has received and
  // handled the report submit request.
  async promiseReportSubmitHandled(requestHandler) {
    if (typeof requestHandler !== "function") {
      throw new Error("requestHandler should be a function");
    }
    return new Promise((resolve, reject) => {
      this._abuseRequestHandlers.unshift({ resolve, reject, requestHandler });
    });
  },

  // Return a promise resolved to the abuse report panel element,
  // once its rendering is completed.
  // If abuseReportEl is undefined, it looks for the currently opened
  // report panel.
  async promiseReportRendered(abuseReportEl) {
    let el = abuseReportEl;

    if (!el) {
      if (AbuseReporter.openDialogDisabled) {
        const frame = this.getReportFrame();
        el = await frame.promiseAbuseReport;
      } else {
        const win = this.getReportDialog();
        if (!win) {
          await waitForNewWindow();
        }

        el = await this.promiseReportDialogRendered();
        ok(el, "Got an abuse report panel");
      }
    }
    return el._radioCheckedReason
      ? el
      : BrowserTestUtils.waitForEvent(
          el,
          "abuse-report:updated",
          "Wait the abuse report panel to be rendered"
        ).then(() => el);
  },

  // A promise resolved when the given abuse report panel element
  // has been rendered. If a panel name ("reasons" or "submit") is
  // passed as a second parameter, it also asserts that the panel is
  // updated to the expected view mode.
  async promiseReportUpdated(abuseReportEl, panel) {
    const evt = await BrowserTestUtils.waitForEvent(
      abuseReportEl,
      "abuse-report:updated",
      "Wait abuse report panel update"
    );

    if (panel) {
      is(evt.detail.panel, panel, `Got a "${panel}" update event`);

      const el = abuseReportEl;
      switch (evt.detail.panel) {
        case "reasons":
          ok(!el._reasonsPanel.hidden, "Reasons panel should be visible");
          ok(el._submitPanel.hidden, "Submit panel should be hidden");
          break;
        case "submit":
          ok(el._reasonsPanel.hidden, "Reasons panel should be hidden");
          ok(!el._submitPanel.hidden, "Submit panel should be visible");
          break;
      }
    }
  },

  // Returns a promise resolved once the expected number of abuse report
  // message bars have been created.
  promiseMessageBars(expectedMessageBarCount) {
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
            "abuse-report:new-message-bar",
            listener
          );
        }
      }
      gHtmlAboutAddonsWindow.document.addEventListener(
        "abuse-report:new-message-bar",
        listener
      );
    });
  },

  // Assert that the report action is hidden on the addon card
  // for the given about:addons windows and extension id.
  async assertReportActionHidden(gManagerWindow, extId) {
    await gManagerWindow.promiseHtmlBrowserLoaded();
    const { contentDocument: doc } = gManagerWindow.getHtmlBrowser();

    let addonCard = doc.querySelector(
      `addon-list addon-card[addon-id="${extId}"]`
    );
    ok(addonCard, `Got the addon-card for the ${extId} test extension`);

    let reportButton = addonCard.querySelector("[action=report]");
    ok(reportButton, `Got the report action for ${extId}`);
    ok(reportButton.hidden, `${extId} report action should be hidden`);
  },

  // Assert that the report panel is hidden (or closed if the report
  // panel is opened in its own dialog window).
  async assertReportPanelHidden() {
    if (AbuseReporter.openDialogDisabled) {
      const abuseReportFrameEl = this.getReportFrame();
      ok(
        abuseReportFrameEl.hidden,
        "Abuse Report frame should be initially hidden"
      );
      return;
    }

    const win = this.getReportDialog();
    ok(!win, "Abuse Report dialog should be initially hidden");
  },

  createMockAddons(mockProviderAddons) {
    this._mockProvider.createAddons(mockProviderAddons);
  },

  async clickPanelButton(buttonEl, { label = undefined } = {}) {
    info(`Clicking the '${buttonEl.textContent.trim() || label}' button`);
    // NOTE: ideally this should synthesize the mouse event,
    // we call the click method to prevent intermittent timeouts
    // due to the mouse event not received by the target element.
    buttonEl.click();
  },

  triggerNewReport(addonId, reportEntryPoint) {
    gHtmlAboutAddonsWindow.openAbuseReport({ addonId, reportEntryPoint });
  },

  triggerSubmit(reason, message) {
    if (AbuseReporter.openDialogDisabled) {
      const el = this.getReportFrame();
      el.handleEvent(
        new CustomEvent("abuse-report:submit", {
          detail: { report: el.report, reason, message },
        })
      );
      return;
    }

    const reportEl = this.getReportDialog().document.querySelector(
      "addon-abuse-report"
    );
    reportEl._form.elements.message.value = message;
    reportEl._form.elements.reason.value = reason;
    reportEl.submit();
  },

  async openReport(addonId, reportEntryPoint = REPORT_ENTRY_POINT) {
    // Close the current about:addons window if it has been leaved open from
    // a previous test case failure.
    if (gHtmlAboutAddonsWindow) {
      await closeAboutAddons();
    }

    await openAboutAddons();

    let promiseReportPanel;

    if (AbuseReporter.openDialogDisabled) {
      const frame = this.getReportFrame();
      promiseReportPanel = frame.promiseAbuseReport.then(el =>
        this.promiseReportRendered(el)
      );
    } else {
      promiseReportPanel = waitForNewWindow().then(() =>
        this.promiseReportDialogRendered()
      );
    }

    this.triggerNewReport(addonId, reportEntryPoint);

    const panelEl = await promiseReportPanel;
    await this.promiseReportRendered(panelEl);
    is(panelEl.addonId, addonId, `Got Abuse Report panel for ${addonId}`);

    return panelEl;
  },

  async closeReportPanel(panelEl) {
    const onceReportClosed = AbuseReportTestUtils.promiseReportClosed(panelEl);

    info("Cancel report and wait the dialog to be closed");
    panelEl.dispatchEvent(new CustomEvent("abuse-report:cancel"));

    await onceReportClosed;
  },

  // Internal helper methods.

  _setupMockProvider() {
    this._mockProvider = new MockProvider();
    this._mockProvider.createAddons([
      {
        id: THEME_NO_UNINSTALL_ID,
        name: "This theme cannot be uninstalled",
        version: "1.1",
        creator: { name: "Theme creator", url: "http://example.com/creator" },
        type: "theme",
        permissions: 0,
      },
      {
        id: EXT_WITH_PRIVILEGED_URL_ID,
        name: "This extension has an unexpected privileged creator URL",
        version: "1.1",
        creator: { name: "creator", url: "about:config" },
        type: "extension",
      },
      {
        id: EXT_SYSTEM_ADDON_ID,
        name: "This is a system addon",
        version: "1.1",
        creator: { name: "creator", url: "http://example.com/creator" },
        type: "extension",
        isSystem: true,
      },
      {
        id: EXT_UNSUPPORTED_TYPE_ADDON_ID,
        name: "This is a fake unsupported addon type",
        version: "1.1",
        type: "unsupported_addon_type",
      },
      {
        id: EXT_LANGPACK_ADDON_ID,
        name: "This is a fake langpack",
        version: "1.1",
        type: "locale",
      },
      {
        id: EXT_DICTIONARY_ADDON_ID,
        name: "This is a fake dictionary",
        version: "1.1",
        type: "dictionary",
      },
    ]);
  },

  _setupMockServer() {
    if (this._mockServer) {
      return;
    }

    // Init test report api server.
    const server = AddonTestUtils.createHttpServer({
      hosts: ["test.addons.org"],
    });
    this._mockServer = server;

    server.registerPathHandler("/api/report/", (request, response) => {
      const stream = request.bodyInputStream;
      const buffer = NetUtil.readInputStream(stream, stream.available());
      const data = new TextDecoder().decode(buffer);
      const promisedHandler = this._abuseRequestHandlers.pop();
      if (promisedHandler) {
        const { requestHandler, resolve, reject } = promisedHandler;
        try {
          requestHandler({ data, request, response });
          resolve();
        } catch (err) {
          ok(false, `Unexpected requestHandler error ${err} ${err.stack}\n`);
          reject(err);
        }
      } else {
        ok(false, `Unexpected request: ${request.path} ${data}`);
      }
    });

    server.registerPrefixHandler("/api/addons/addon/", (request, response) => {
      const addonId = request.path.split("/").pop();
      if (!this.amoAddonDetailsMap.has(addonId)) {
        response.setStatusLine(request.httpVersion, 404, "Not Found");
        response.write(JSON.stringify({ detail: "Not found." }));
      } else {
        response.setStatusLine(request.httpVersion, 200, "Success");
        response.write(JSON.stringify(this.amoAddonDetailsMap.get(addonId)));
      }
    });
    server.registerPathHandler(
      "/assets/fake-icon-url.png",
      (request, response) => {
        response.setStatusLine(request.httpVersion, 200, "Success");
        response.write("");
        response.finish();
      }
    );
  },
};

function createPromptConfirmEx({
  remove = false,
  report = false,
  expectCheckboxHidden = false,
} = {}) {
  return (...args) => {
    const checkboxState = args.pop();
    const checkboxMessage = args.pop();
    is(
      checkboxState && checkboxState.value,
      false,
      "checkboxState should be initially false"
    );
    if (expectCheckboxHidden) {
      ok(
        !checkboxMessage,
        "Should not have a checkboxMessage in promptService.confirmEx call"
      );
    } else {
      ok(
        checkboxMessage,
        "Got a checkboxMessage in promptService.confirmEx call"
      );
    }

    // Report checkbox selected.
    checkboxState.value = report;

    // Remove accepted.
    return remove ? 0 : 1;
  };
}

/**
 * Test tasks shared between browser_html_abuse_report.js and
 * browser_html_abuse_report_dialog.js.
 *
 * NOTE: add new shared test tasks to addCommonAbuseReportTestTasks.
 */

// This test case verified that the abuse report panels contains a radio
// button for all the expected "abuse report reasons", they are grouped
// together under the same form field named "reason".
async function test_abusereport_issuelist() {
  const extension = await installTestExtension();

  const abuseReportEl = await AbuseReportTestUtils.openReport(extension.id);

  const reasonsPanel = abuseReportEl._reasonsPanel;
  const radioButtons = reasonsPanel.querySelectorAll("[type=radio]");
  const selectedRadios = reasonsPanel.querySelectorAll("[type=radio]:checked");

  is(selectedRadios.length, 1, "Expect only one radio button selected");
  is(
    selectedRadios[0],
    radioButtons[0],
    "Expect the first radio button to be selected"
  );

  is(
    abuseReportEl.reason,
    radioButtons[0].value,
    `The reason property has the expected value: ${radioButtons[0].value}`
  );

  const reasons = Array.from(radioButtons).map(el => el.value);
  Assert.deepEqual(
    reasons.sort(),
    AbuseReportTestUtils.getReasons(abuseReportEl).sort(),
    `Got a radio button for the expected reasons`
  );

  for (const radio of radioButtons) {
    const reasonInfo = AbuseReportTestUtils.getReasonInfo(
      abuseReportEl,
      radio.value
    );
    const expectExampleHidden =
      reasonInfo && reasonInfo.isExampleHidden("extension");
    is(
      radio.parentNode.querySelector(".reason-example").hidden,
      expectExampleHidden,
      `Got expected visibility on the example for reason "${radio.value}"`
    );
  }

  info("Change the selected reason to " + radioButtons[3].value);
  radioButtons[3].checked = true;
  is(
    abuseReportEl.reason,
    radioButtons[3].value,
    "The reason property has the expected value"
  );

  await extension.unload();
  await closeAboutAddons();
}

// This test case verifies that the abuse report panel:
// - switches from its "reasons list" mode to its "submit report" mode when the
//   "next" button is clicked
// - goes back to the "reasons list" mode when the "go back" button is clicked
// - the abuse report panel is closed when the "close" icon is clicked
async function test_abusereport_submitpanel() {
  const extension = await installTestExtension();

  const abuseReportEl = await AbuseReportTestUtils.openReport(extension.id);

  ok(
    !abuseReportEl._reasonsPanel.hidden,
    "The list of abuse reasons is the currently visible"
  );
  ok(
    abuseReportEl._submitPanel.hidden,
    "The submit panel is the currently hidden"
  );

  let onceUpdated = AbuseReportTestUtils.promiseReportUpdated(
    abuseReportEl,
    "submit"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnNext);
  await onceUpdated;

  onceUpdated = AbuseReportTestUtils.promiseReportUpdated(
    abuseReportEl,
    "reasons"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnGoBack);
  await onceUpdated;

  const onceReportClosed = AbuseReportTestUtils.promiseReportClosed(
    abuseReportEl
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnCancel);
  await onceReportClosed;

  await extension.unload();
  await closeAboutAddons();
}

// This test case verifies that the abuse report panel sends the expected data
// in the "abuse-report:submit" event detail.
async function test_abusereport_submit() {
  // Reset the timestamp of the last report between tests.
  AbuseReporter._lastReportTimestamp = null;
  const extension = await installTestExtension();

  const abuseReportEl = await AbuseReportTestUtils.openReport(extension.id);

  ok(
    !abuseReportEl._reasonsPanel.hidden,
    "The list of abuse reasons is the currently visible"
  );

  let onceUpdated = AbuseReportTestUtils.promiseReportUpdated(
    abuseReportEl,
    "submit"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnNext);
  await onceUpdated;

  is(abuseReportEl.message, "", "The abuse report message is initially empty");

  info("Test typing a message in the abuse report submit panel textarea");
  const typedMessage = "Description of the extension abuse report";

  EventUtils.synthesizeComposition(
    {
      data: typedMessage,
      type: "compositioncommit",
    },
    abuseReportEl.ownerGlobal
  );

  is(
    abuseReportEl.message,
    typedMessage,
    "Got the expected typed message in the abuse report"
  );

  const expectedDetail = {
    addonId: extension.id,
  };

  const expectedReason = abuseReportEl.reason;
  const expectedMessage = abuseReportEl.message;

  function handleSubmitRequest({ request, response }) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/json", false);
    response.write("{}");
  }

  let reportSubmitted;
  const onReportSubmitted = AbuseReportTestUtils.promiseReportSubmitHandled(
    ({ data, request, response }) => {
      reportSubmitted = JSON.parse(data);
      handleSubmitRequest({ request, response });
    }
  );

  const onceReportClosed = AbuseReportTestUtils.promiseReportClosed(
    abuseReportEl
  );

  const onMessageBarsCreated = AbuseReportTestUtils.promiseMessageBars(2);

  const onceSubmitEvent = BrowserTestUtils.waitForEvent(
    abuseReportEl,
    "abuse-report:submit"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnSubmit);
  const submitEvent = await onceSubmitEvent;

  const actualDetail = {
    addonId: submitEvent.detail.addonId,
  };
  Assert.deepEqual(
    actualDetail,
    expectedDetail,
    "Got the expected detail in the abuse-report:submit event"
  );

  ok(
    submitEvent.detail.report,
    "Got a report object in the abuse-report:submit event detail"
  );

  // Verify that, when the "abuse-report:submit" has been sent,
  // the abuse report panel has been hidden, the report has been
  // submitted and the expected  message bar is created in the
  // HTML about:addons page.
  info("Wait the report to be submitted to the api server");
  await onReportSubmitted;
  info("Wait the report panel to be closed");
  await onceReportClosed;

  is(
    reportSubmitted.addon,
    ADDON_ID,
    "Got the expected addon in the submitted report"
  );
  is(
    reportSubmitted.reason,
    expectedReason,
    "Got the expected reason in the submitted report"
  );
  is(
    reportSubmitted.message,
    expectedMessage,
    "Got the expected message in the submitted report"
  );
  is(
    reportSubmitted.report_entry_point,
    REPORT_ENTRY_POINT,
    "Got the expected report_entry_point in the submitted report"
  );

  info("Waiting the expected message bars to be created");
  const barDetails = await onMessageBarsCreated;
  is(barDetails.length, 2, "Expect two message bars to have been created");
  is(
    barDetails[0].definitionId,
    "submitting",
    "Got a submitting message bar as expected"
  );
  is(
    barDetails[1].definitionId,
    "submitted",
    "Got a submitted message bar as expected"
  );

  await extension.unload();
  await closeAboutAddons();
}

// This test case verifies that the abuse report panel contains the expected
// suggestions when the selected reason requires it (and urls are being set
// on the links elements included in the suggestions when expected).
async function test_abusereport_suggestions(addonId) {
  const addon = await AddonManager.getAddonByID(addonId);

  const abuseReportEl = await AbuseReportTestUtils.openReport(addonId);

  const {
    _btnNext,
    _btnGoBack,
    _reasonsPanel,
    _submitPanel,
    _submitPanel: { _suggestions },
  } = abuseReportEl;

  for (const reason of AbuseReportTestUtils.getReasons(abuseReportEl)) {
    const reasonInfo = AbuseReportTestUtils.getReasonInfo(
      abuseReportEl,
      reason
    );

    if (reasonInfo.isReasonHidden(addon.type)) {
      continue;
    }

    info(`Test suggestions for abuse reason "${reason}"`);

    // Select a reason with suggestions.
    let radioEl = abuseReportEl.querySelector(`#abuse-reason-${reason}`);
    ok(radioEl, `Found radio button for "${reason}"`);
    radioEl.checked = true;

    let oncePanelUpdated = AbuseReportTestUtils.promiseReportUpdated(
      abuseReportEl,
      "submit"
    );
    await AbuseReportTestUtils.clickPanelButton(_btnNext);
    await oncePanelUpdated;

    const localizedSuggestionsContent = Array.from(
      _suggestions.querySelectorAll("[data-l10n-id]")
    ).filter(el => !el.hidden);

    is(
      !_suggestions.hidden,
      !!reasonInfo.hasSuggestions,
      `Suggestions block has the expected visibility for "${reason}"`
    );
    if (reasonInfo.hasSuggestions) {
      ok(
        !!localizedSuggestionsContent.length,
        `Category suggestions should not be empty for "${reason}"`
      );
    } else {
      ok(
        localizedSuggestionsContent.length === 0,
        `Category suggestions should be empty for "${reason}"`
      );
    }

    const extSupportLink = _suggestions.querySelector(
      ".extension-support-link"
    );
    if (extSupportLink) {
      is(
        extSupportLink.getAttribute("href"),
        BASE_TEST_MANIFEST.homepage_url,
        "Got the expected extension-support-url"
      );
    }

    const learnMoreLinks = [];
    for (const linkClass of _suggestions.LEARNMORE_LINKS) {
      learnMoreLinks.push(..._suggestions.querySelectorAll(linkClass));
    }

    if (learnMoreLinks.length) {
      ok(
        learnMoreLinks.every(el => el.getAttribute("target") === "_blank"),
        "All the learn more links have target _blank"
      );
      ok(
        learnMoreLinks.every(el => el.hasAttribute("href")),
        "All the learn more links have a url set"
      );
    }

    oncePanelUpdated = AbuseReportTestUtils.promiseReportUpdated(
      abuseReportEl,
      "reasons"
    );
    await AbuseReportTestUtils.clickPanelButton(_btnGoBack);
    await oncePanelUpdated;
    ok(!_reasonsPanel.hidden, "Reasons panel should be visible");
    ok(_submitPanel.hidden, "Submit panel should be hidden");
  }

  await closeAboutAddons();
}

async function test_abusereport_suggestions_extension() {
  const EXT_ID = "test-extension-suggestions@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  await test_abusereport_suggestions(EXT_ID);
  await extension.unload();
}

async function test_abusereport_suggestions_theme() {
  const THEME_ID = "theme@mochi.test";
  const theme = await installTestExtension(THEME_ID, "theme");
  await test_abusereport_suggestions(THEME_ID);
  await theme.unload();
}

// This test case verifies the message bars created on other
// scenarios (e.g. report creation and submissions errors).
async function test_abusereport_messagebars() {
  const EXT_ID = "test-extension-report@mochi.test";
  const EXT_ID2 = "test-extension-report-2@mochi.test";
  const THEME_ID = "test-theme-report@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  const extension2 = await installTestExtension(EXT_ID2);
  const theme = await installTestExtension(THEME_ID, "theme");

  async function assertMessageBars(
    expectedMessageBarIds,
    testSetup,
    testMessageBarDetails
  ) {
    await openAboutAddons();
    const expectedLength = expectedMessageBarIds.length;
    const onMessageBarsCreated = AbuseReportTestUtils.promiseMessageBars(
      expectedLength
    );
    // Reset the timestamp of the last report between tests.
    AbuseReporter._lastReportTimestamp = null;
    await testSetup();
    info(`Waiting for ${expectedLength} message-bars to be created`);
    const barDetails = await onMessageBarsCreated;
    Assert.deepEqual(
      barDetails.map(d => d.definitionId),
      expectedMessageBarIds,
      "Got the expected message bars"
    );
    if (testMessageBarDetails) {
      await testMessageBarDetails(barDetails);
    }
    await closeAboutAddons();
  }

  function setTestRequestHandler(responseStatus, responseData) {
    AbuseReportTestUtils.promiseReportSubmitHandled(({ request, response }) => {
      response.setStatusLine(request.httpVersion, responseStatus, "Error");
      response.write(responseData);
    });
  }

  await assertMessageBars(["ERROR_ADDON_NOTFOUND"], async () => {
    info("Test message bars on addon not found");
    AbuseReportTestUtils.triggerNewReport(
      "non-existend-addon-id@mochi.test",
      REPORT_ENTRY_POINT
    );
  });

  await assertMessageBars(["submitting", "ERROR_RECENT_SUBMIT"], async () => {
    info("Test message bars on recent submission");
    const promiseRendered = AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await promiseRendered;
    AbuseReporter.updateLastReportTimestamp();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_ABORTED_SUBMIT"], async () => {
    info("Test message bars on aborted submission");
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await AbuseReportTestUtils.promiseReportRendered();
    const { _report } = AbuseReportTestUtils.getReportPanel();
    _report.abort();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_SERVER"], async () => {
    info("Test message bars on server error");
    setTestRequestHandler(500);
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_CLIENT"], async () => {
    info("Test message bars on client error");
    setTestRequestHandler(400);
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_UNKNOWN"], async () => {
    info("Test message bars on unexpected status code");
    setTestRequestHandler(604);
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  await assertMessageBars(["submitting", "ERROR_UNKNOWN"], async () => {
    info("Test message bars on invalid json in the response data");
    setTestRequestHandler(200, "");
    AbuseReportTestUtils.triggerNewReport(EXT_ID, REPORT_ENTRY_POINT);
    await AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  // Verify message bar on add-on without perm_can_uninstall.
  await assertMessageBars(
    ["submitting", "submitted-no-remove-action"],
    async () => {
      info("Test message bars on report submitted on an addon without remove");
      setTestRequestHandler(200, "{}");
      AbuseReportTestUtils.triggerNewReport(THEME_NO_UNINSTALL_ID, "menu");
      await AbuseReportTestUtils.promiseReportRendered();
      AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
    }
  );

  // Verify the 3 expected entry points:
  //   menu, toolbar_context_menu and uninstall
  // (See https://addons-server.readthedocs.io/en/latest/topics/api/abuse.html).
  await assertMessageBars(["submitting", "submitted"], async () => {
    info("Test message bars on report opened from addon options menu");
    setTestRequestHandler(200, "{}");
    AbuseReportTestUtils.triggerNewReport(EXT_ID, "menu");
    await AbuseReportTestUtils.promiseReportRendered();
    AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
  });

  for (const extId of [EXT_ID, THEME_ID]) {
    await assertMessageBars(
      ["submitting", "submitted"],
      async () => {
        info(`Test message bars on ${extId} reported from toolbar contextmenu`);
        setTestRequestHandler(200, "{}");
        AbuseReportTestUtils.triggerNewReport(extId, "toolbar_context_menu");
        await AbuseReportTestUtils.promiseReportRendered();
        AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
      },
      ([submittingDetails, submittedDetails]) => {
        const buttonsL10nId = Array.from(
          submittedDetails.messagebar.querySelectorAll("button")
        ).map(el => el.getAttribute("data-l10n-id"));
        if (extId === THEME_ID) {
          ok(
            buttonsL10nId.every(id => id.endsWith("-theme")),
            "submitted bar actions should use the Fluent id for themes"
          );
        } else {
          ok(
            buttonsL10nId.every(id => id.endsWith("-extension")),
            "submitted bar actions should use the Fluent id for extensions"
          );
        }
      }
    );
  }

  for (const extId of [EXT_ID2, THEME_ID]) {
    const testFn = async () => {
      info(`Test message bars on ${extId} reported opened from addon removal`);
      setTestRequestHandler(200, "{}");
      AbuseReportTestUtils.triggerNewReport(extId, "uninstall");
      await AbuseReportTestUtils.promiseReportRendered();
      const addon = await AddonManager.getAddonByID(extId);
      // Ensure that the test extension is pending uninstall as it would be
      // when a user trigger this scenario on an actual addon uninstall.
      await addon.uninstall(true);
      AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
    };
    await assertMessageBars(["submitting", "submitted-and-removed"], testFn);
  }

  await extension.unload();
  await extension2.unload();
  await theme.unload();
}

async function test_abusereport_from_aboutaddons_menu() {
  const EXT_ID = "test-report-from-aboutaddons-menu@mochi.test";
  const extension = await installTestExtension(EXT_ID);

  await openAboutAddons();
  await gManagerWindow.promiseHtmlBrowserLoaded();

  AbuseReportTestUtils.assertReportPanelHidden();

  const { contentDocument: doc } = gManagerWindow.getHtmlBrowser();

  const addonCard = doc.querySelector(
    `addon-list addon-card[addon-id="${extension.id}"]`
  );
  ok(addonCard, "Got the addon-card for the test extension");

  const reportButton = addonCard.querySelector("[action=report]");
  ok(reportButton, "Got the report action for the test extension");

  info("Click the report action and wait for the 'abuse-report:new' event");

  let onceReportOpened = AbuseReportTestUtils.promiseReportOpened({
    addonId: extension.id,
    reportEntryPoint: "menu",
  });
  reportButton.click();
  const panelEl = await onceReportOpened;

  await AbuseReportTestUtils.closeReportPanel(panelEl);

  await closeAboutAddons();
  await extension.unload();
}

async function test_abusereport_from_aboutaddons_remove() {
  const EXT_ID = "test-report-from-aboutaddons-remove@mochi.test";

  // Test on a theme addon to cover the report checkbox included in the
  // uninstall dialog also on a theme.
  const extension = await installTestExtension(EXT_ID, "theme");

  await openAboutAddons("theme");
  await gManagerWindow.promiseHtmlBrowserLoaded();

  AbuseReportTestUtils.assertReportPanelHidden();

  const { contentDocument: doc } = gManagerWindow.getHtmlBrowser();

  const addonCard = doc.querySelector(
    `addon-list addon-card[addon-id="${extension.id}"]`
  );
  ok(addonCard, "Got the addon-card for the test theme extension");

  const removeButton = addonCard.querySelector("[action=remove]");
  ok(removeButton, "Got the remove action for the test theme extension");

  // Prepare the mocked prompt service.
  const promptService = mockPromptService();
  promptService.confirmEx = createPromptConfirmEx({
    remove: true,
    report: true,
  });

  info("Click the report action and wait for the 'abuse-report:new' event");

  const onceReportOpened = AbuseReportTestUtils.promiseReportOpened({
    addonId: extension.id,
    reportEntryPoint: "uninstall",
  });
  removeButton.click();
  const panelEl = await onceReportOpened;

  await AbuseReportTestUtils.closeReportPanel(panelEl);

  await closeAboutAddons();
  await extension.unload();
}

async function test_abusereport_from_browserAction_remove() {
  const EXT_ID = "test-report-from-browseraction-remove@mochi.test";
  const xpiFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      ...BASE_TEST_MANIFEST,
      browser_action: {},
      applications: { gecko: { id: EXT_ID } },
    },
  });
  const addon = await AddonManager.installTemporaryAddon(xpiFile);

  const buttonId = `${makeWidgetId(EXT_ID)}-browser-action`;

  async function promiseAnimationFrame() {
    await new Promise(resolve => window.requestAnimationFrame(resolve));

    let { tm } = Services;
    return new Promise(resolve => tm.dispatchToMainThread(resolve));
  }

  async function reportFromContextMenuRemove() {
    const menu = document.getElementById("toolbar-context-menu");
    const node = document.getElementById(CSS.escape(buttonId));
    const shown = BrowserTestUtils.waitForEvent(
      menu,
      "popupshown",
      "Wair for contextmenu popup"
    );

    // Wait for an animation frame as we do for the other mochitest-browser
    // tests related to the browserActions.
    await promiseAnimationFrame();
    EventUtils.synthesizeMouseAtCenter(node, { type: "contextmenu" });
    await shown;

    info(`Clicking on "Remove Extension" context menu item`);
    let removeExtension = menu.querySelector(
      ".customize-context-removeExtension"
    );
    removeExtension.click();

    return menu;
  }

  // Prepare the mocked prompt service.
  const promptService = mockPromptService();
  promptService.confirmEx = createPromptConfirmEx({
    remove: true,
    report: true,
  });

  await BrowserTestUtils.withNewTab("about:blank", async function() {
    info(`Open browserAction context menu in toolbar context menu`);
    let promiseMenu = reportFromContextMenuRemove();

    // Wait about:addons to be loaded.
    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.browserLoaded(browser);

    let onceReportOpened = AbuseReportTestUtils.promiseReportOpened({
      addonId: EXT_ID,
      reportEntryPoint: "uninstall",
      managerWindow: browser.contentWindow,
    });

    is(
      browser.currentURI.spec,
      "about:addons",
      "about:addons tab currently selected"
    );

    let menu = await promiseMenu;
    menu.hidePopup();

    let panelEl = await onceReportOpened;
    await AbuseReportTestUtils.closeReportPanel(panelEl);

    let onceExtStarted = AddonTestUtils.promiseWebExtensionStartup(EXT_ID);
    addon.cancelUninstall();
    await onceExtStarted;

    // Reload the tab to verify Bug 1559124 didn't regressed.
    browser.contentWindow.location.reload();
    await BrowserTestUtils.browserLoaded(browser);
    is(
      browser.currentURI.spec,
      "about:addons",
      "about:addons tab currently selected"
    );

    onceReportOpened = AbuseReportTestUtils.promiseReportOpened({
      addonId: EXT_ID,
      reportEntryPoint: "uninstall",
      managerWindow: browser.contentWindow,
    });

    menu = await reportFromContextMenuRemove();
    info("Wait for the report panel");
    panelEl = await onceReportOpened;

    info("Wait for the report panel to be closed");
    await AbuseReportTestUtils.closeReportPanel(panelEl);

    menu.hidePopup();

    onceExtStarted = AddonTestUtils.promiseWebExtensionStartup(EXT_ID);
    addon.cancelUninstall();
    await onceExtStarted;
  });

  await addon.uninstall();
}

async function test_no_broken_suggestion_on_missing_supportURL() {
  const EXT_ID = "test-no-author@mochi.test";
  const extension = await installTestExtension(EXT_ID, "extension", {
    homepage_url: undefined,
  });

  const abuseReportEl = await AbuseReportTestUtils.openReport(EXT_ID);

  info("Select broken as the abuse reason");
  abuseReportEl.querySelector("#abuse-reason-broken").checked = true;

  let oncePanelUpdated = AbuseReportTestUtils.promiseReportUpdated(
    abuseReportEl,
    "submit"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnNext);
  await oncePanelUpdated;

  const suggestionEl = abuseReportEl.querySelector(
    "abuse-report-reason-suggestions"
  );
  is(suggestionEl.reason, "broken", "Got the expected suggestion element");
  ok(suggestionEl.hidden, "suggestion element should be empty");

  await closeAboutAddons();
  await extension.unload();
}

// This test verify that the abuse report panel is opening the
// author link using a null triggeringPrincipal.
async function test_abusereport_open_author_url() {
  const abuseReportEl = await AbuseReportTestUtils.openReport(
    EXT_WITH_PRIVILEGED_URL_ID
  );

  const authorLink = abuseReportEl._linkAddonAuthor;
  ok(authorLink, "Got the author link element");
  is(
    authorLink.href,
    "about:config",
    "Got a privileged url in the link element"
  );

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        // eslint-disable-next-line max-len
        message: /Security Error: Content at moz-nullprincipal:{.*} may not load or link to about:config/,
      },
    ]);
  });

  let tabSwitched = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  authorLink.click();
  await tabSwitched;

  is(
    gBrowser.selectedBrowser.currentURI.spec,
    "about:blank",
    "Got about:blank loaded in the new tab"
  );

  SimpleTest.endMonitorConsole();
  await waitForConsole;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await closeAboutAddons();
}

async function test_report_action_hidden_on_builtin_addons() {
  await openAboutAddons("theme");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    DEFAULT_BUILTIN_THEME_ID
  );
  await closeAboutAddons();
}

async function test_report_action_hidden_on_system_addons() {
  await openAboutAddons("extension");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_SYSTEM_ADDON_ID
  );
  await closeAboutAddons();
}

async function test_report_action_hidden_on_dictionary_addons() {
  await openAboutAddons("dictionary");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_DICTIONARY_ADDON_ID
  );
  await closeAboutAddons();
}

async function test_report_action_hidden_on_langpack_addons() {
  await openAboutAddons("locale");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_LANGPACK_ADDON_ID
  );
  await closeAboutAddons();
}

// This test verifies that triggering a report that would be immediately
// cancelled (e.g. because abuse reports for that extension type are not
// supported) the abuse report frame is being hidden as expected.
async function test_report_hidden_on_report_unsupported_addontype() {
  await openAboutAddons();

  // The error message bar is only being shown in the new implementation
  // which opens the report in a dialog window, on the contrary when the
  // report is shown in a about:addons subframe the report is cancelled
  // but no error messagebar is going to be shown.
  let onceCreateReportFailed;
  let reportFrameEl;
  if (AbuseReporter.openDialogDisabled) {
    reportFrameEl = AbuseReportTestUtils.getReportFrame();
    onceCreateReportFailed = BrowserTestUtils.waitForEvent(
      reportFrameEl,
      "abuse-report:cancel"
    );
  } else {
    onceCreateReportFailed = AbuseReportTestUtils.promiseMessageBars(1);
  }

  AbuseReportTestUtils.triggerNewReport(EXT_UNSUPPORTED_TYPE_ADDON_ID, "menu");

  await onceCreateReportFailed;

  if (AbuseReporter.openDialogDisabled) {
    is(reportFrameEl.hidden, true, "report frame should not be visible");
  } else {
    is(
      AbuseReporter.getOpenDialog(),
      undefined,
      "report dialog should not be open"
    );
  }

  await closeAboutAddons();
}

async function test_no_report_checkbox_for_unsupported_addon_types() {
  async function test_report_checkbox_hidden(addon) {
    await openAboutAddons(addon.type);
    await gManagerWindow.promiseHtmlBrowserLoaded();

    const abuseReportFrameEl = AbuseReportTestUtils.getReportFrame();
    ok(abuseReportFrameEl.hidden, "Abuse Report frame should be hidden");

    const { contentDocument: doc } = gManagerWindow.getHtmlBrowser();

    const addonCard = doc.querySelector(
      `addon-list addon-card[addon-id="${addon.id}"]`
    );
    ok(addonCard, "Got the addon-card for the test extension");

    const removeButton = addonCard.querySelector("[action=remove]");
    ok(removeButton, "Got the remove action for the test extension");

    // Prepare the mocked prompt service.
    const promptService = mockPromptService();
    promptService.confirmEx = createPromptConfirmEx({
      remove: true,
      report: false,
      expectCheckboxHidden: true,
    });

    info("Click the report action and wait for the addon to be removed");
    const promiseCardRemoved = BrowserTestUtils.waitForEvent(
      addonCard.closest("addon-list"),
      "remove"
    );
    removeButton.click();
    await promiseCardRemoved;

    ok(abuseReportFrameEl.hidden, "Abuse Report frame should still be hidden");

    await closeAboutAddons();
  }

  const reportNotSupportedAddons = [
    {
      id: "fake-langpack-to-remove@mochi.test",
      name: "This is a fake langpack",
      version: "1.1",
      type: "locale",
    },
    {
      id: "fake-dictionary-to-remove@mochi.test",
      name: "This is a fake dictionary",
      version: "1.1",
      type: "dictionary",
    },
  ];

  AbuseReportTestUtils.createMockAddons(reportNotSupportedAddons);

  for (const { id } of reportNotSupportedAddons) {
    const addon = await AddonManager.getAddonByID(id);
    await test_report_checkbox_hidden(addon);
  }
}

async function test_author_hidden_when_missing() {
  const EXT_ID = "test-no-author@mochi.test";
  const extension = await installTestExtension(EXT_ID, "extension", {
    author: undefined,
  });

  const abuseReportEl = await AbuseReportTestUtils.openReport(EXT_ID);

  const addon = await AddonManager.getAddonByID(EXT_ID);

  ok(!addon.creator, "addon.creator should not be undefined");
  ok(
    abuseReportEl._addonAuthorContainer.hidden,
    "author container should be hidden"
  );

  await closeAboutAddons();
  await extension.unload();
}

// NOTE: this function should add all the common test tasks (defined
// in this support file) that should run in both browser_html_abuse_report.js
// and browser_html_abuse_report_dialog.js.
function addCommonAbuseReportTestTasks() {
  // Base tests on panel webcomponents.
  add_task(test_abusereport_issuelist);
  add_task(test_abusereport_submitpanel);
  add_task(test_abusereport_submit);
  add_task(test_abusereport_messagebars);
  add_task(test_abusereport_suggestions_extension);
  add_task(test_abusereport_suggestions_theme);
  add_task(test_abusereport_from_aboutaddons_menu);
  add_task(test_abusereport_from_aboutaddons_remove);
  add_task(test_abusereport_from_browserAction_remove);
  add_task(test_abusereport_open_author_url);

  // Test report action hidden on non-supported extension types.
  add_task(test_report_action_hidden_on_builtin_addons);
  add_task(test_report_action_hidden_on_system_addons);
  add_task(test_report_action_hidden_on_dictionary_addons);
  add_task(test_report_action_hidden_on_langpack_addons);
  add_task(test_report_hidden_on_report_unsupported_addontype);

  // Test regression fixes.
  add_task(test_author_hidden_when_missing);
  add_task(test_no_report_checkbox_for_unsupported_addon_types);
  add_task(test_no_broken_suggestion_on_missing_supportURL);
}
