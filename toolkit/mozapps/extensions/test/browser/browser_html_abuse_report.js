/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_setup(async function () {
  // Make sure the integrated abuse report panel is the one enabled
  // while this test file runs (instead of the AMO hosted form).
  // NOTE: behaviors expected when amoFormEnabled is true are tested
  // in the separate browser_amo_abuse_report.js test file.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.amoFormEnabled", false]],
  });
  await AbuseReportTestUtils.setup();
});

/**
 * Base tests on abuse report panel webcomponents.
 */

// This test case verified that the abuse report panels contains a radio
// button for all the expected "abuse report reasons", they are grouped
// together under the same form field named "reason".
add_task(async function test_abusereport_issuelist() {
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
});

// This test case verifies that the abuse report panel:
// - switches from its "reasons list" mode to its "submit report" mode when the
//   "next" button is clicked
// - goes back to the "reasons list" mode when the "go back" button is clicked
// - the abuse report panel is closed when the "close" icon is clicked
add_task(async function test_abusereport_submitpanel() {
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
  const MozButtonGroup =
    abuseReportEl.ownerGlobal.customElements.get("moz-button-group");

  ok(MozButtonGroup, "Expect MozButtonGroup custom element to be defined");

  const assertButtonInMozButtonGroup = (
    btnEl,
    { expectPrimary = false } = {}
  ) => {
    // Let's include the l10n id into the assertion messages,
    // to make it more likely to be immediately clear which
    // button hit a failure if any of the following assertion
    // fails.
    let l10nId = btnEl.getAttribute("data-l10n-id");
    is(
      btnEl.classList.contains("primary"),
      expectPrimary,
      `Expect button ${l10nId} to have${
        expectPrimary ? "" : " NOT"
      } the primary class set`
    );

    ok(
      btnEl.parentElement instanceof MozButtonGroup,
      `Expect button ${l10nId} to be slotted inside the expected custom element`
    );

    is(
      btnEl.getAttribute("slot"),
      expectPrimary ? "primary" : null,
      `Expect button ${l10nId} slot to ${
        expectPrimary ? "" : "NOT "
      } be set to primary`
    );
  };

  // Verify button group from the initial panel.
  assertButtonInMozButtonGroup(abuseReportEl._btnNext, { expectPrimary: true });
  assertButtonInMozButtonGroup(abuseReportEl._btnCancel, {
    expectPrimary: false,
  });
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnNext);
  await onceUpdated;
  // Verify button group from the submit panel mode.
  assertButtonInMozButtonGroup(abuseReportEl._btnSubmit, {
    expectPrimary: true,
  });
  assertButtonInMozButtonGroup(abuseReportEl._btnGoBack, {
    expectPrimary: false,
  });
  onceUpdated = AbuseReportTestUtils.promiseReportUpdated(
    abuseReportEl,
    "reasons"
  );
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnGoBack);
  await onceUpdated;

  const onceReportClosed =
    AbuseReportTestUtils.promiseReportClosed(abuseReportEl);
  await AbuseReportTestUtils.clickPanelButton(abuseReportEl._btnCancel);
  await onceReportClosed;

  await extension.unload();
  await closeAboutAddons();
});

// This test case verifies that the abuse report panel sends the expected data
// in the "abuse-report:submit" event detail.
add_task(async function test_abusereport_submit() {
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

  const onceReportClosed =
    AbuseReportTestUtils.promiseReportClosed(abuseReportEl);

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
});

// This helper does verify that the abuse report panel contains the expected
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

    // TODO(Bug 1789718): Remove after the deprecated XPIProvider-based
    // implementation is also removed.
    const addonType =
      addon.type === "sitepermission-deprecated"
        ? "sitepermission"
        : addon.type;

    if (reasonInfo.isReasonHidden(addonType)) {
      continue;
    }

    info(`Test suggestions for abuse reason "${reason}"`);

    // Select a reason with suggestions.
    let radioEl = abuseReportEl.querySelector(`#abuse-reason-${reason}`);
    ok(radioEl, `Found radio button for "${reason}"`);
    radioEl.checked = true;

    // Make sure the element localization is completed before
    // checking the content isn't empty.
    await document.l10n.translateFragment(radioEl);

    // Verify each radio button has a non-empty localized string.
    const localizedRadioContent = Array.from(
      radioEl.closest("label").querySelectorAll("[data-l10n-id]")
    ).filter(el => !el.hidden);

    for (let el of localizedRadioContent) {
      isnot(
        el.textContent,
        "",
        `Fluent string id '${el.getAttribute("data-l10n-id")}' missing`
      );
    }

    // Switch to the submit form with the current reason radio selected.
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
    learnMoreLinks.push(
      ..._suggestions.querySelectorAll(
        'a[is="moz-support-link"], .abuse-policy-learnmore'
      )
    );

    if (learnMoreLinks.length) {
      is(
        _suggestions.querySelectorAll(
          'a[is="moz-support-link"]:not([support-page])'
        ).length,
        0,
        "Every SUMO link should point to a specific page"
      );
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

add_task(async function test_abusereport_suggestions_extension() {
  const EXT_ID = "test-extension-suggestions@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  await test_abusereport_suggestions(EXT_ID);
  await extension.unload();
});

add_task(async function test_abusereport_suggestions_theme() {
  const THEME_ID = "theme@mochi.test";
  const theme = await installTestExtension(THEME_ID, "theme");
  await test_abusereport_suggestions(THEME_ID);
  await theme.unload();
});

// TODO(Bug 1789718): adapt to SitePermAddonProvider implementation.
add_task(async function test_abusereport_suggestions_sitepermission() {
  const SITEPERM_ADDON_ID = "webmidi@mochi.test";
  const sitePermAddon = await installTestExtension(
    SITEPERM_ADDON_ID,
    "sitepermission-deprecated"
  );
  await test_abusereport_suggestions(SITEPERM_ADDON_ID);
  await sitePermAddon.unload();
});

// This test case verifies the message bars created on other
// scenarios (e.g. report creation and submissions errors).
//
// TODO(Bug 1789718): adapt to SitePermAddonProvider implementation.
add_task(async function test_abusereport_messagebars() {
  const EXT_ID = "test-extension-report@mochi.test";
  const EXT_ID2 = "test-extension-report-2@mochi.test";
  const THEME_ID = "test-theme-report@mochi.test";
  const SITEPERM_ADDON_ID = "webmidi-report@mochi.test";
  const extension = await installTestExtension(EXT_ID);
  const extension2 = await installTestExtension(EXT_ID2);
  const theme = await installTestExtension(THEME_ID, "theme");
  const sitePermAddon = await installTestExtension(
    SITEPERM_ADDON_ID,
    "sitepermission-deprecated"
  );

  async function assertMessageBars(
    expectedMessageBarIds,
    testSetup,
    testMessageBarDetails
  ) {
    await openAboutAddons();
    const expectedLength = expectedMessageBarIds.length;
    const onMessageBarsCreated =
      AbuseReportTestUtils.promiseMessageBars(expectedLength);
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

  for (const extId of [EXT_ID2, THEME_ID, SITEPERM_ADDON_ID]) {
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
    const assertMessageBarDetails = async ([
      submittingDetails,
      submittedDetails,
    ]) => AbuseReportTestUtils.assertFluentStrings(submittedDetails.messagebar);
    await assertMessageBars(
      ["submitting", "submitted-and-removed"],
      testFn,
      assertMessageBarDetails
    );
  }

  // Verify message bar on sitepermission add-on type.
  await assertMessageBars(
    ["submitting", "submitted"],
    async () => {
      info(
        "Test message bars for report submitted on an sitepermission addon type"
      );
      setTestRequestHandler(200, "{}");
      AbuseReportTestUtils.triggerNewReport(SITEPERM_ADDON_ID, "menu");
      await AbuseReportTestUtils.promiseReportRendered();
      AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");
    },
    ([submittingDetails, submittedDetails]) =>
      AbuseReportTestUtils.assertFluentStrings(submittedDetails.messagebar)
  );

  await extension.unload();
  await extension2.unload();
  await theme.unload();
  await sitePermAddon.unload();
});

add_task(async function test_abusereport_from_aboutaddons_menu() {
  const EXT_ID = "test-report-from-aboutaddons-menu@mochi.test";
  const extension = await installTestExtension(EXT_ID);

  await openAboutAddons();

  AbuseReportTestUtils.assertReportPanelHidden();

  const addonCard = gManagerWindow.document.querySelector(
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
});

add_task(async function test_abusereport_from_aboutaddons_remove() {
  const EXT_ID = "test-report-from-aboutaddons-remove@mochi.test";

  // Test on a theme addon to cover the report checkbox included in the
  // uninstall dialog also on a theme.
  const extension = await installTestExtension(EXT_ID, "theme");

  await openAboutAddons("theme");

  AbuseReportTestUtils.assertReportPanelHidden();

  const addonCard = gManagerWindow.document.querySelector(
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
});

add_task(async function test_abusereport_from_browserAction_remove() {
  const EXT_ID = "test-report-from-browseraction-remove@mochi.test";
  const xpiFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      ...BASE_TEST_MANIFEST,
      browser_action: {
        default_area: "navbar",
      },
      browser_specific_settings: { gecko: { id: EXT_ID } },
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

  await BrowserTestUtils.withNewTab("about:blank", async function () {
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
});

/*
 * Test report action hidden on non-supported extension types.
 */
add_task(async function test_report_action_hidden_on_builtin_addons() {
  await openAboutAddons("theme");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    DEFAULT_BUILTIN_THEME_ID
  );
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_system_addons() {
  await openAboutAddons("extension");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_SYSTEM_ADDON_ID
  );
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_dictionary_addons() {
  await openAboutAddons("dictionary");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_DICTIONARY_ADDON_ID
  );
  await closeAboutAddons();
});

add_task(async function test_report_action_hidden_on_langpack_addons() {
  await openAboutAddons("locale");
  await AbuseReportTestUtils.assertReportActionHidden(
    gManagerWindow,
    EXT_LANGPACK_ADDON_ID
  );
  await closeAboutAddons();
});

// This test verifies that triggering a report that would be immediately
// cancelled (e.g. because abuse reports for that extension type are not
// supported) the abuse report is being hidden as expected.
add_task(async function test_report_hidden_on_report_unsupported_addontype() {
  await openAboutAddons();

  let onceCreateReportFailed = AbuseReportTestUtils.promiseMessageBars(1);

  AbuseReportTestUtils.triggerNewReport(EXT_UNSUPPORTED_TYPE_ADDON_ID, "menu");

  await onceCreateReportFailed;

  ok(!AbuseReporter.getOpenDialog(), "report dialog should not be open");

  await closeAboutAddons();
});

/*
 * Test regression fixes.
 */

add_task(async function test_no_broken_suggestion_on_missing_supportURL() {
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
});

// This test verify that the abuse report panel is opening the
// author link using a null triggeringPrincipal.
add_task(async function test_abusereport_open_author_url() {
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
        message:
          // eslint-disable-next-line max-len
          /Security Error: Content at moz-nullprincipal:{.*} may not load or link to about:config/,
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
});

add_task(async function test_no_report_checkbox_for_unsupported_addon_types() {
  async function test_report_checkbox_hidden(addon) {
    await openAboutAddons(addon.type);

    const addonCard = gManagerWindow.document.querySelector(
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
});

add_task(async function test_author_hidden_when_missing() {
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
});

// Verify addon.siteOrigin is used as a fallback when homepage_url/developer.url
// or support url are missing.
//
// TODO(Bug 1789718): adapt to SitePermAddonProvider implementation.
add_task(async function test_siteperm_siteorigin_fallback() {
  const SITEPERM_ADDON_ID = "webmidi-site-origin@mochi.test";
  const sitePermAddon = await installTestExtension(
    SITEPERM_ADDON_ID,
    "sitepermission-deprecated",
    {
      homepage_url: undefined,
    }
  );

  const abuseReportEl = await AbuseReportTestUtils.openReport(
    SITEPERM_ADDON_ID
  );
  const addon = await AddonManager.getAddonByID(SITEPERM_ADDON_ID);

  ok(addon.siteOrigin, "addon.siteOrigin should not be undefined");
  ok(!addon.supportURL, "addon.supportURL should not be set");
  ok(!addon.homepageURL, "addon.homepageURL should not be set");
  is(
    abuseReportEl.supportURL,
    addon.siteOrigin,
    "Got the expected support_url"
  );

  await closeAboutAddons();
  await sitePermAddon.unload();
});
