/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

/* exported installTestExtension, addCommonAbuseReportTestTasks,
 *          createPromptConfirmEx, DEFAULT_BUILTIN_THEME_ID,
 *          gManagerWindow, handleSubmitRequest, makeWidgetId,
 *          waitForNewWindow, waitClosedWindow, AbuseReporter,
 *          AbuseReporterTestUtils, AddonTestUtils
 */

/* global MockProvider, loadInitialView, closeView */

const { AbuseReporter } = ChromeUtils.importESModule(
  "resource://gre/modules/AbuseReporter.sys.mjs"
);
const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs"
);

const { makeWidgetId } = ExtensionCommon;

const ADDON_ID = "test-extension-to-report@mochi.test";
const REPORT_ENTRY_POINT = "menu";
const BASE_TEST_MANIFEST = {
  name: "Fake extension to report",
  author: "Fake author",
  homepage_url: "https://fake.extension.url/",
};
const DEFAULT_BUILTIN_THEME_ID = "default-theme@mozilla.org";
const EXT_DICTIONARY_ADDON_ID = "fake-dictionary@mochi.test";
const EXT_LANGPACK_ADDON_ID = "fake-langpack@mochi.test";
const EXT_WITH_PRIVILEGED_URL_ID = "ext-with-privileged-url@mochi.test";
const EXT_SYSTEM_ADDON_ID = "test-system-addon@mochi.test";
const EXT_UNSUPPORTED_TYPE_ADDON_ID = "report-unsupported-type@mochi.test";
const THEME_NO_UNINSTALL_ID = "theme-without-perm-can-uninstall@mochi.test";

let gManagerWindow;

AddonTestUtils.initMochitest(this);

async function openAboutAddons(type = "extension") {
  gManagerWindow = await loadInitialView(type);
}

async function closeAboutAddons() {
  if (gManagerWindow) {
    await closeView(gManagerWindow);
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
  let additionalProps = {
    icons: {
      32: "test-icon.png",
    },
  };

  switch (type) {
    case "theme":
      additionalProps = {
        ...additionalProps,
        theme: {
          colors: {
            frame: "#a14040",
            tab_background_text: "#fac96e",
          },
        },
      };
      break;

    // TODO(Bug 1789718): Remove after the deprecated XPIProvider-based
    // implementation is also removed.
    case "sitepermission-deprecated":
      additionalProps = {
        name: "WebMIDI test addon for https://mochi.test",
        install_origins: ["https://mochi.test"],
        site_permissions: ["midi"],
      };
      break;
    case "extension":
      break;
    default:
      throw new Error(`Unexpected addon type: ${type}`);
  }

  const extensionOpts = {
    manifest: {
      ...BASE_TEST_MANIFEST,
      ...additionalProps,
      ...manifest,
      browser_specific_settings: { gecko: { id } },
    },
    useAddonManager: "temporary",
  };

  // TODO(Bug 1789718): Remove after the deprecated XPIProvider-based
  // implementation is also removed.
  if (type === "sitepermission-deprecated") {
    const xpi = AddonTestUtils.createTempWebExtensionFile(extensionOpts);
    const addon = await AddonManager.installTemporaryAddon(xpi);
    // The extension object that ExtensionTestUtils.loadExtension returns for
    // mochitest is pretty tight to the Extension class, and so for now this
    // returns a more minimal `extension` test object which only provides the
    // `unload` method.
    //
    // For the purpose of the abuse reports tests that are using this helper
    // this should be already enough.
    return {
      addon,
      unload: () => addon.uninstall(),
    };
  }

  const extension = ExtensionTestUtils.loadExtension(extensionOpts);
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

  // Returns the currently open abuse report dialog window (if any).
  getReportDialog() {
    return Services.ww.getWindowByName("addons-abuse-report-dialog");
  },

  // Returns the parameters related to the report dialog (if any).
  getReportDialogParams() {
    const win = this.getReportDialog();
    return win && win.arguments[0] && win.arguments[0].wrappedJSObject;
  },

  // Returns a reference to the addon-abuse-report element from the currently
  // open abuse report.
  getReportPanel() {
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

    if (!this.getReportDialog()) {
      info("Wait for the report dialog window");
      const dialog = await waitForNewWindow();
      is(dialog, this.getReportDialog(), "Report dialog opened");
    }

    info("Wait for the abuse report panel render");
    abuseReportEl = await AbuseReportTestUtils.promiseReportDialogRendered();

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
  // is closed.
  // Also asserts that a specific report panel element has been closed,
  // if one has been provided through the optional panel parameter.
  async promiseReportClosed(panel) {
    const win = panel ? panel.ownerGlobal : this.getReportDialog();
    if (!win || win.closed) {
      throw Error("Expected report dialog not found or already closed");
    }

    await waitClosedWindow(win);
    // Assert that the panel has been closed (if the caller has passed it).
    if (panel) {
      ok(!panel.ownerGlobal, "abuse report dialog closed");
    }
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
      const win = this.getReportDialog();
      if (!win) {
        await waitForNewWindow();
      }

      el = await this.promiseReportDialogRendered();
      ok(el, "Got an abuse report panel");
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
        if (gManagerWindow) {
          gManagerWindow.document.removeEventListener(
            "abuse-report:new-message-bar",
            listener
          );
        }
      }
      gManagerWindow.document.addEventListener(
        "abuse-report:new-message-bar",
        listener
      );
    });
  },

  async assertFluentStrings(containerEl) {
    // Make sure all localized elements have defined Fluent strings.
    let localizedEls = Array.from(
      containerEl.querySelectorAll("[data-l10n-id]")
    );
    if (containerEl.getAttribute("data-l10n-id")) {
      localizedEls.push(containerEl);
    }
    ok(localizedEls.length, "Got localized elements");
    for (let el of localizedEls) {
      const l10nId = el.getAttribute("data-l10n-id");
      const l10nAttrs = el.getAttribute("data-l10n-attrs");
      if (!l10nAttrs) {
        await TestUtils.waitForCondition(
          () => el.textContent !== "",
          `Element with Fluent id '${l10nId}' should not be empty`
        );
      } else {
        await TestUtils.waitForCondition(
          () => el.message !== "",
          `Message attribute of the element with Fluent id '${l10nId}' 
          should not be empty`
        );
      }
    }
  },

  // Assert that the report action visibility on the addon card
  // for the given about:addons windows and extension id.
  async assertReportActionVisibility(gManagerWindow, extId, expectShown) {
    let addonCard = gManagerWindow.document.querySelector(
      `addon-list addon-card[addon-id="${extId}"]`
    );
    ok(addonCard, `Got the addon-card for the ${extId} test extension`);

    let reportButton = addonCard.querySelector("[action=report]");
    ok(reportButton, `Got the report action for ${extId}`);
    Assert.equal(
      reportButton.hidden,
      !expectShown,
      `${extId} report action should be ${expectShown ? "shown" : "hidden"}`
    );
  },

  // Assert that the report action is hidden on the addon card
  // for the given about:addons windows and extension id.
  assertReportActionHidden(gManagerWindow, extId) {
    return this.assertReportActionVisibility(gManagerWindow, extId, false);
  },

  // Assert that the report action is shown on the addon card
  // for the given about:addons windows and extension id.
  assertReportActionShown(gManagerWindow, extId) {
    return this.assertReportActionVisibility(gManagerWindow, extId, true);
  },

  // Assert that the report panel is hidden (or closed if the report
  // panel is opened in its own dialog window).
  async assertReportPanelHidden() {
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
    gManagerWindow.openAbuseReport({ addonId, reportEntryPoint });
  },

  triggerSubmit(reason, message) {
    const reportEl =
      this.getReportDialog().document.querySelector("addon-abuse-report");
    reportEl._form.elements.message.value = message;
    reportEl._form.elements.reason.value = reason;
    reportEl.submit();
  },

  async openReport(addonId, reportEntryPoint = REPORT_ENTRY_POINT) {
    // Close the current about:addons window if it has been leaved open from
    // a previous test case failure.
    if (gManagerWindow) {
      await closeAboutAddons();
    }

    await openAboutAddons();

    let promiseReportPanel = waitForNewWindow().then(() =>
      this.promiseReportDialogRendered()
    );

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
