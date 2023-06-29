const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { getFxAccountsSingleton } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
const { FirefoxRelayTelemetry } = ChromeUtils.importESModule(
  "resource://gre/modules/FirefoxRelayTelemetry.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const gFxAccounts = getFxAccountsSingleton();
let gRelayACOptionsTitles;
let gHttpServer;

const TEST_URL_PATH = `https://example.org${DIRECTORY_PATH}form_basic_signup.html`;

const MOCK_MASKS = [
  {
    full_address: "email1@mozilla.com",
    description: "Email 1 Description",
    enabled: true,
  },
  {
    full_address: "email2@mozilla.com",
    description: "Email 2 Description",
    enabled: false,
  },
  {
    full_address: "email3@mozilla.com",
    description: "Email 3 Description",
    enabled: true,
  },
];

const SERVER_SCENARIOS = {
  free_tier_limit: {
    "/relayaddresses/": {
      POST: (request, response) => {
        response.setStatusLine(request.httpVersion, 403);
        response.write(JSON.stringify({ error_code: "free_tier_limit" }));
      },
      GET: (_, response) => {
        response.write(JSON.stringify(MOCK_MASKS));
      },
    },
  },
  unknown_error: {
    "/relayaddresses/": {
      default: (request, response) => {
        response.setStatusLine(request.httpVersion, 408);
      },
    },
  },

  default: {
    default: (request, response) => {
      response.setStatusLine(request.httpVersion, 200);
      response.write(JSON.stringify({ foo: "bar" }));
    },
  },
};

const simpleRouter = scenarioName => (request, response) => {
  const routeHandler =
    SERVER_SCENARIOS[scenarioName][request._path] ?? SERVER_SCENARIOS.default;
  const methodHandler =
    routeHandler?.[request._method] ??
    routeHandler.default ??
    SERVER_SCENARIOS.default.default;
  methodHandler(request, response);
};
const setupServerScenario = (scenarioName = "default") =>
  gHttpServer.registerPrefixHandler("/", simpleRouter(scenarioName));

const setupRelayScenario = async scenarioName => {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.firefoxRelay.feature", scenarioName]],
  });
  Services.telemetry.clearEvents();
};

const waitForEvents = async expectedEvents =>
  TestUtils.waitForCondition(
    () => {
      const snapshots = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      );

      return (snapshots.parent?.length ?? 0) >= (expectedEvents.length ?? 0);
    },
    "Wait for telemetry to be collected",
    100,
    100
  );

async function assertEvents(expectedEvents) {
  // To avoid intermittent failures, we wait for telemetry to be collected
  await waitForEvents(expectedEvents);
  const events = TelemetryTestUtils.getEvents(
    { category: "relay_integration" },
    { process: "parent" }
  );

  for (let i = 0; i < expectedEvents.length; i++) {
    const keysInExpectedEvent = Object.keys(expectedEvents[i]);
    keysInExpectedEvent.forEach(key => {
      const assertFn =
        typeof events[i][key] === "object"
          ? Assert.deepEqual.bind(Assert)
          : Assert.equal.bind(Assert);
      assertFn(
        events[i][key],
        expectedEvents[i][key],
        `Key value for ${key} should match`
      );
    });
  }
}

async function openRelayAC(browser) {
  // In rare cases, especially in chaos mode in verify tests, some events creep in.
  // Clear them out before we start.
  Services.telemetry.clearEvents();
  const popup = document.getElementById("PopupAutoComplete");
  await openACPopup(popup, browser, "#form-basic-username");
  const popupItem = document
    .querySelector("richlistitem")
    .getAttribute("ac-label");
  const popupItemTitle = JSON.parse(popupItem).title;

  Assert.ok(
    gRelayACOptionsTitles.some(title => title.value === popupItemTitle),
    "AC Popup has an item Relay option shown in popup"
  );

  const promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.firstChild.getItemAtIndex(0).click();
  await promiseHidden;
}

add_setup(async function () {
  await ExperimentAPI.ready();
  const cleanupExperiment = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "password-autocomplete",
      value: { firefoxRelayIntegration: true },
    },
    { isRollout: true }
  );

  gHttpServer = new HttpServer();
  setupServerScenario();

  gHttpServer.start(-1);

  const API_ENDPOINT = `http://localhost:${gHttpServer.identity.primaryPort}/`;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.firefoxRelay.feature", "available"],
      ["signon.firefoxRelay.base_url", API_ENDPOINT],
    ],
  });

  sinon.stub(gFxAccounts, "hasLocalSession").returns(true);
  sinon
    .stub(gFxAccounts.constructor.config, "isProductionConfig")
    .returns(true);
  sinon.stub(gFxAccounts, "getOAuthToken").returns("MOCK_TOKEN");
  sinon.stub(gFxAccounts, "getSignedInUser").returns({
    email: "example@mozilla.com",
  });

  const canRecordExtendedOld = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("relay_integration", true);

  gRelayACOptionsTitles = await new Localization([
    "browser/firefoxRelay.ftl",
    "toolkit/branding/brandings.ftl",
  ]).formatMessages([
    "firefox-relay-opt-in-title-1",
    "firefox-relay-use-mask-title",
  ]);

  registerCleanupFunction(async () => {
    await cleanupExperiment();
    await new Promise(resolve => {
      gHttpServer.stop(function () {
        resolve();
      });
    });
    Services.telemetry.setEventRecordingEnabled("relay_integration", false);
    Services.telemetry.clearEvents();
    Services.telemetry.canRecordExtended = canRecordExtendedOld;
    sinon.restore();
  });
});

add_task(async function test_pref_toggle() {
  await setupRelayScenario("available");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:preferences#privacy",
    },
    async function (browser) {
      const relayIntegrationCheckbox = content.document.querySelector(
        "checkbox#relayIntegration"
      );
      relayIntegrationCheckbox.click();
      relayIntegrationCheckbox.click();
      await assertEvents([
        { object: "pref_change", method: "disabled" },
        { object: "pref_change", method: "enabled" },
      ]);
    }
  );
});

add_task(async function test_popup_option_optin_enabled() {
  await setupRelayScenario("available");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-primary-button")
        .click();

      await notificationHidden;

      await BrowserTestUtils.waitForEvent(
        ConfirmationHint._panel,
        "popuphidden"
      );

      await assertEvents([
        {
          object: "offer_relay",
          method: "shown",
          extra: { scenario: "SignUpFormScenario" },
        },
        {
          object: "offer_relay",
          method: "clicked",
          extra: { scenario: "SignUpFormScenario" },
        },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "enabled" },
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "0" },
        },
      ]);
    }
  );
});

add_task(async function test_popup_option_optin_postponed() {
  await setupRelayScenario("available");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-secondary-button")
        .click();

      await notificationHidden;

      await assertEvents([
        { object: "offer_relay", method: "shown" },
        { object: "offer_relay", method: "clicked" },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "postponed" },
      ]);
    }
  );
});

add_task(async function test_popup_option_optin_disabled() {
  await setupRelayScenario("available");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;
      const menupopup = notificationPopup.querySelector("menupopup");
      const menuitem = menupopup.querySelector("menuitem");

      menuitem.click();
      await notificationHidden;

      await assertEvents([
        { object: "offer_relay", method: "shown" },
        { object: "offer_relay", method: "clicked" },
        { object: "opt_in_panel", method: "shown" },
        { object: "opt_in_panel", method: "disabled" },
      ]);
    }
  );
});

add_task(async function test_popup_option_fillusername() {
  await setupRelayScenario("enabled");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      await BrowserTestUtils.waitForEvent(
        ConfirmationHint._panel,
        "popuphidden"
      );
      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
      ]);
    }
  );
});

add_task(async function test_fillusername_free_tier_limit() {
  await setupRelayScenario("enabled");
  setupServerScenario("free_tier_limit");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);

      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;
      notificationPopup.querySelector(".reusable-relay-masks button").click();
      await notificationHidden;

      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "free_tier_limit" },
        },
        {
          object: "reuse_panel",
          method: "shown",
        },
        {
          object: "reuse_panel",
          method: "reuse_mask",
        },
      ]);

      await SpecialPowers.spawn(browser, [], async function () {
        const username = content.document.getElementById("form-basic-username");
        Assert.equal(
          username.value,
          "email1@mozilla.com",
          "Username field should be filled with the first mask"
        );
      });
    }
  );
});

add_task(async function test_fillusername_error() {
  await setupRelayScenario("enabled");
  setupServerScenario("unknown_error");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);

      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );

      await notificationShown;
      Assert.equal(
        notificationPopup.querySelector("popupnotification").id,
        "relay-integration-error-notification",
        "Error message should be displayed"
      );

      await assertEvents([
        { object: "fill_username", method: "shown" },
        {
          object: "fill_username",
          method: "clicked",
        },
        {
          object: "reuse_panel",
          method: "shown",
          extra: { error_code: "408" },
        },
      ]);
    }
  );
});

add_task(async function test_auth_token_error() {
  setupRelayScenario("enabled");
  gFxAccounts.getOAuthToken.restore();
  const oauthTokenStub = sinon.stub(gFxAccounts, "getOAuthToken").throws();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function (browser) {
      await openRelayAC(browser);
      const notificationPopup = document.getElementById("notification-popup");
      const notificationShown = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "shown"
      );
      const notificationHidden = BrowserTestUtils.waitForPopupEvent(
        notificationPopup,
        "hidden"
      );

      await notificationShown;

      notificationPopup
        .querySelector("button.popup-notification-primary-button")
        .click();

      await notificationHidden;

      await assertEvents([
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "0" },
        },
        {
          object: "fill_username",
          method: "clicked",
          extra: { error_code: "0" },
        },
        {
          object: "fill_username",
          method: "shown",
          extra: { error_code: "418" },
        },
      ]);
    }
  );
  oauthTokenStub.restore();
});
