/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const XPI_URL = `${SECURE_TESTROOT}../xpinstall/amosigned.xpi`;
const XPI_ADDON_ID = "amosigned-xpi@tests.mozilla.org";

AddonTestUtils.initMochitest(this);

AddonTestUtils.hookAMTelemetryEvents();

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
    ],
  });

  PermissionTestUtils.add(
    "https://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  registerCleanupFunction(async () => {
    PermissionTestUtils.remove("https://example.com", "install");
    await SpecialPowers.popPrefEnv();
  });
});

async function testInstallTrigger(
  msg,
  tabURL,
  contentFnArgs,
  contentFn,
  expectedTelemetryInfo,
  expectBlockedOrigin
) {
  // Clear collected events before each test, otherwise the test would fail
  // intermittently when Glean is going to submit the events and clear them
  // after reaching the max events length limit.
  Services.fog.testResetFOG();

  await BrowserTestUtils.withNewTab(tabURL, async browser => {
    if (expectBlockedOrigin) {
      const promiseOriginBlocked = TestUtils.topicObserved(
        "addon-install-origin-blocked"
      );
      await SpecialPowers.spawn(browser, contentFnArgs, contentFn);
      const [subject] = await promiseOriginBlocked;
      const installId = subject.wrappedJSObject.installs[0].installId;

      let gleanEvents = AddonTestUtils.getAMGleanEvents("install", {
        install_id: `${installId}`,
        step: "site_blocked",
      });
      ok(!!gleanEvents.length, "Found Glean events for the blocked install.");
      Assert.deepEqual(
        { source: gleanEvents[0].source },
        expectedTelemetryInfo,
        `Got expected Glean telemetry on test case "${msg}"`
      );

      // Select all telemetry events related to the installId.
      const telemetryEvents = AddonTestUtils.getAMTelemetryEvents().filter(
        ev => {
          return (
            ev.method === "install" &&
            ev.value === `${installId}` &&
            ev.extra.step === "site_blocked"
          );
        }
      );
      ok(
        !!telemetryEvents.length,
        "Found telemetry events for the blocked install"
      );

      const source = telemetryEvents[0]?.extra.source;
      Assert.deepEqual(
        { source },
        expectedTelemetryInfo,
        `Got expected telemetry on test case "${msg}"`
      );
      return;
    }

    let installPromptPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    ).then(panel => {
      panel.button.click();
    });

    let promptPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      XPI_ADDON_ID
    );

    await SpecialPowers.spawn(browser, contentFnArgs, contentFn);

    await Promise.all([installPromptPromise, promptPromise]);

    let addon = await promiseAddonByID(XPI_ADDON_ID);

    registerCleanupFunction(async () => {
      await addon.uninstall();
    });

    // Check that the expected installTelemetryInfo has been stored in the
    // addon details.
    AddonTestUtils.checkInstallInfo(
      addon,
      { method: "installTrigger", ...expectedTelemetryInfo },
      `on "${msg}"`
    );

    await addon.uninstall();
  });
}

add_task(function testInstallAfterHistoryPushState() {
  return testInstallTrigger(
    "InstallTrigger after history.pushState",
    SECURE_TESTROOT,
    [SECURE_TESTROOT, XPI_URL],
    (secureTestRoot, xpiURL) => {
      // `sourceURL` should match the exact location, even after a location
      // update using the history API. In this case, we update the URL with
      // query parameters and expect `sourceURL` to contain those parameters.
      content.history.pushState(
        {}, // state
        "", // title
        `${secureTestRoot}?some=query&par=am`
      );
      content.InstallTrigger.install({ URL: xpiURL });
    },
    {
      source: "test-host",
      sourceURL:
        "https://example.com/browser/toolkit/mozapps/extensions/test/browser/?some=query&par=am",
    }
  );
});

add_task(async function testInstallTriggerFromSubframe() {
  function runTestCase(msg, tabURL, testFrameAttrs, expected) {
    info(
      `InstallTrigger from iframe test: ${msg} - frame attributes ${JSON.stringify(
        testFrameAttrs
      )}`
    );
    return testInstallTrigger(
      msg,
      tabURL,
      [XPI_URL, testFrameAttrs],
      async (xpiURL, frameAttrs) => {
        const frame = content.document.createElement("iframe");
        if (frameAttrs) {
          for (const attr of Object.keys(frameAttrs)) {
            let value = frameAttrs[attr];
            if (value === "blob:") {
              const blob = new content.Blob(["blob-testpage"]);
              value = content.URL.createObjectURL(blob, "text/html");
            }
            frame[attr] = value;
          }
        }
        const promiseLoaded = new Promise(resolve =>
          frame.addEventListener("load", resolve, { once: true })
        );
        content.document.body.appendChild(frame);
        await promiseLoaded;
        frame.contentWindow.InstallTrigger.install({ URL: xpiURL });
      },
      expected.telemetryInfo,
      expected.blockedOrigin
    );
  }

  // On Windows "file:///" does not load the default files index html page
  // and the test would get stuck.
  const fileURL = AppConstants.platform === "win" ? "file:///C:/" : "file:///";

  const expected = {
    http: {
      telemetryInfo: {
        source: "test-host",
        sourceURL:
          "https://example.com/browser/toolkit/mozapps/extensions/test/browser/",
      },
      blockedOrigin: false,
    },
    httpBlob: {
      telemetryInfo: {
        source: "test-host",
        // Example: "blob:https://example.com/BLOB_URL_UUID"
        sourceURL: /^blob:https:\/\/example\.com\//,
      },
      blockedOrigin: false,
    },
    file: {
      telemetryInfo: {
        source: "unknown",
        sourceURL: fileURL,
      },
      blockedOrigin: false,
    },
    fileBlob: {
      telemetryInfo: {
        source: "unknown",
        // Example: "blob:null/BLOB_URL_UUID"
        sourceURL: /^blob:null\//,
      },
      blockedOrigin: false,
    },
    httpBlockedOnOrigin: {
      telemetryInfo: {
        source: "test-host",
      },
      blockedOrigin: true,
    },
    otherBlockedOnOrigin: {
      telemetryInfo: {
        source: "unknown",
      },
      blockedOrigin: true,
    },
  };

  const testCases = [
    ["blank iframe with no attributes", SECURE_TESTROOT, {}, expected.http],

    // These are blocked by a Firefox doorhanger and the user can't allow it neither.
    [
      "http page iframe src='blob:...'",
      SECURE_TESTROOT,
      { src: "blob:" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "file page iframe src='blob:...'",
      fileURL,
      { src: "blob:" },
      expected.otherBlockedOnOrigin,
    ],
    [
      "iframe srcdoc=''",
      SECURE_TESTROOT,
      { srcdoc: "" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "blank iframe embedded into a top-level sandbox page",
      `${SECURE_TESTROOT}sandboxed.html`,
      {},
      expected.httpBlockedOnOrigin,
    ],
    [
      "blank iframe with sandbox='allow-scripts'",
      SECURE_TESTROOT,
      { sandbox: "allow-scripts" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "iframe srcdoc='' sandbox='allow-scripts'",
      SECURE_TESTROOT,
      { srcdoc: "", sandbox: "allow-scripts" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "http page iframe src='blob:...' sandbox='allow-scripts'",
      SECURE_TESTROOT,
      { src: "blob:", sandbox: "allow-scripts" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "iframe src='data:...'",
      SECURE_TESTROOT,
      { src: "data:text/html,data-testpage" },
      expected.httpBlockedOnOrigin,
    ],
    [
      "blank frame embedded in a data url",
      "data:text/html,data-testpage",
      {},
      expected.otherBlockedOnOrigin,
    ],
    [
      "blank frame embedded into a about:blank page",
      "about:blank",
      {},
      expected.otherBlockedOnOrigin,
    ],
  ];

  for (const testCase of testCases) {
    await runTestCase(...testCase);
  }
});

add_task(function testInstallBlankFrameNestedIntoBlobURLPage() {
  return testInstallTrigger(
    "Blank frame nested into a blob url page",
    SECURE_TESTROOT,
    [XPI_URL],
    async xpiURL => {
      const url = content.URL.createObjectURL(
        new content.Blob(["blob-testpage"]),
        "text/html"
      );
      const topframe = content.document.createElement("iframe");
      topframe.src = url;
      const topframeLoaded = new Promise(resolve => {
        topframe.addEventListener("load", resolve, { once: true });
      });
      content.document.body.appendChild(topframe);
      await topframeLoaded;
      const subframe = topframe.contentDocument.createElement("iframe");
      topframe.contentDocument.body.appendChild(subframe);
      subframe.contentWindow.InstallTrigger.install({ URL: xpiURL });
    },
    {
      source: "test-host",
    },
    /* expectBlockedOrigin */ true
  );
});

add_task(function testInstallTriggerTopLevelDataURL() {
  return testInstallTrigger(
    "Blank frame nested into a blob url page",
    "data:text/html,testpage",
    [XPI_URL],
    async xpiURL => {
      this.content.InstallTrigger.install({ URL: xpiURL });
    },
    {
      source: "unknown",
    },
    /* expectBlockedOrigin */ true
  );
});

add_task(function teardown_clearUnexamitedTelemetry() {
  // Clear collected telemetry events when we are not going to run any assertion on them.
  // (otherwise the test will fail because of unexamined telemetry events).
  AddonTestUtils.getAMTelemetryEvents();
});
