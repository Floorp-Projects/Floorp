"use strict";

AddonTestUtils.init(this);

const PREF_DNR_FEEDBACK_DEFAULT_VALUE = Services.prefs.getBoolPref(
  "extensions.dnr.feedback",
  false
);

async function testAvailability({
  allowDNRFeedback = false,
  testExpectations,
  ...extensionData
}) {
  function background(testExpectations) {
    let {
      declarativeNetRequest_available = false,
      testMatchOutcome_available = false,
    } = testExpectations;
    browser.test.assertEq(
      declarativeNetRequest_available,
      !!browser.declarativeNetRequest,
      "declarativeNetRequest API namespace availability"
    );
    browser.test.assertEq(
      testMatchOutcome_available,
      !!browser.declarativeNetRequest?.testMatchOutcome,
      "declarativeNetRequest.testMatchOutcome availability"
    );
    browser.test.sendMessage("done");
  }
  let extension = ExtensionTestUtils.loadExtension({
    ...extensionData,
    manifest: {
      manifest_version: 3,
      ...extensionData.manifest,
    },
    background: `(${background})(${JSON.stringify(testExpectations)});`,
  });
  Services.prefs.setBoolPref("extensions.dnr.feedback", allowDNRFeedback);
  try {
    await extension.startup();
    await extension.awaitMessage("done");
    await extension.unload();
  } finally {
    Services.prefs.clearUserPref("extensions.dnr.feedback");
  }
}

add_setup(async () => {
  // TODO bug 1782685: Remove this check.
  Assert.equal(
    Services.prefs.getBoolPref("extensions.dnr.enabled", false),
    false,
    "DNR is disabled by default"
  );
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
});

// Verifies that DNR is disabled by default (until true in bug 1782685).
add_task(
  {
    pref_set: [["extensions.dnr.enabled", false]],
  },
  async function dnr_disabled_by_default() {
    let { messages } = await promiseConsoleOutput(async () => {
      await testAvailability({
        allowDNRFeedback: PREF_DNR_FEEDBACK_DEFAULT_VALUE,
        testExpectations: {
          declarativeNetRequest_available: false,
        },
        manifest: {
          permissions: [
            "declarativeNetRequest",
            "declarativeNetRequestFeedback",
            "declarativeNetRequestWithHostAccess",
          ],
        },
      });
    });

    AddonTestUtils.checkMessages(messages, {
      expected: [
        {
          message: /Reading manifest: Invalid extension permission: declarativeNetRequest$/,
        },
        {
          message: /Reading manifest: Invalid extension permission: declarativeNetRequestFeedback/,
        },
        {
          message: /Reading manifest: Invalid extension permission: declarativeNetRequestWithHostAccess/,
        },
      ],
    });
  }
);

add_task(async function dnr_feedback_apis_disabled_by_default() {
  let { messages } = await promiseConsoleOutput(async () => {
    await testAvailability({
      allowDNRFeedback: PREF_DNR_FEEDBACK_DEFAULT_VALUE,
      testExpectations: {
        declarativeNetRequest_available: true,
      },
      manifest: {
        permissions: [
          "declarativeNetRequest",
          "declarativeNetRequestFeedback",
          "declarativeNetRequestWithHostAccess",
        ],
      },
    });
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      {
        message: /Reading manifest: Invalid extension permission: declarativeNetRequestFeedback/,
      },
    ],
    forbidden: [
      {
        message: /Reading manifest: Invalid extension permission: declarativeNetRequest$/,
      },
      {
        message: /Reading manifest: Invalid extension permission: declarativeNetRequestWithHostAccess/,
      },
    ],
  });
});

// TODO bug 1782685: Remove "min_manifest_version":3 from DNR permissions.
add_task(async function dnr_restricted_to_mv3() {
  let { messages } = await promiseConsoleOutput(async () => {
    // Manifest version-restricted permissions result in schema-generated
    // warnings. Don't fail when the "unrecognized" permission appear, to allow
    // us to check for warning log messages below.
    ExtensionTestUtils.failOnSchemaWarnings(false);
    await testAvailability({
      allowDNRFeedback: true,
      testExpectations: {
        declarativeNetRequest_available: false,
      },
      manifest: {
        manifest_version: 2,
        permissions: [
          "declarativeNetRequest",
          "declarativeNetRequestFeedback",
          "declarativeNetRequestWithHostAccess",
        ],
      },
    });
    ExtensionTestUtils.failOnSchemaWarnings(true);
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      {
        message: /Warning processing permissions: Error processing permissions.0: Value "declarativeNetRequest"/,
      },
      {
        message: /Warning processing permissions: Error processing permissions.1: Value "declarativeNetRequestFeedback"/,
      },
      {
        message: /Warning processing permissions: Error processing permissions.2: Value "declarativeNetRequestWithHostAccess"/,
      },
    ],
  });
});

add_task(async function with_declarativeNetRequest_permission() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      declarativeNetRequest_available: true,
      // feature allowed, but missing declarativeNetRequestFeedback:
      testMatchOutcome_available: false,
    },
    manifest: {
      permissions: ["declarativeNetRequest"],
    },
  });
});

add_task(async function with_declarativeNetRequestWithHostAccess_permission() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      declarativeNetRequest_available: true,
      // feature allowed, but missing declarativeNetRequestFeedback:
      testMatchOutcome_available: false,
    },
    manifest: {
      permissions: ["declarativeNetRequestWithHostAccess"],
    },
  });
});

add_task(async function with_all_declarativeNetRequest_permissions() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      declarativeNetRequest_available: true,
      // feature allowed, but missing declarativeNetRequestFeedback:
      testMatchOutcome_available: false,
    },
    manifest: {
      permissions: [
        "declarativeNetRequest",
        "declarativeNetRequestWithHostAccess",
      ],
    },
  });
});

add_task(async function no_declarativeNetRequest_permission() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      // Just declarativeNetRequestFeedback should not unlock the API.
      declarativeNetRequest_available: false,
    },
    manifest: {
      permissions: ["declarativeNetRequestFeedback"],
    },
  });
});

add_task(async function with_declarativeNetRequestFeedback_permission() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      declarativeNetRequest_available: true,
      // feature allowed, and all permissions specified:
      testMatchOutcome_available: true,
    },
    manifest: {
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    },
  });
});

add_task(async function declarativeNetRequestFeedback_without_feature() {
  await testAvailability({
    allowDNRFeedback: false,
    testExpectations: {
      declarativeNetRequest_available: true,
      // all permissions set, but DNR feedback feature not allowed.
      testMatchOutcome_available: false,
    },
    manifest: {
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    },
  });
});
