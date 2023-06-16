"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionDNRLimits: "resource://gre/modules/ExtensionDNRLimits.sys.mjs",
});

AddonTestUtils.init(this);

const PREF_DNR_FEEDBACK_DEFAULT_VALUE = Services.prefs.getBoolPref(
  "extensions.dnr.feedback",
  false
);

// To distinguish from testMatchOutcome, undefined vs resolving vs rejecting.
const kTestMatchOutcomeNotAllowed = "kTestMatchOutcomeNotAllowed";

async function testAvailability({
  allowDNRFeedback = false,
  testExpectations,
  ...extensionData
}) {
  async function background(testExpectations) {
    let {
      // declarativeNetRequest should be available if "declarativeNetRequest" or
      // "declarativeNetRequestWithHostAccess" permission is requested.
      //  (and always unavailable when "extensions.dnr.enabled" pref is false)
      declarativeNetRequest_available = false,
      // testMatchOutcome is available when the "declarativeNetRequestFeedback"
      // permission is granted AND the "extensions.dnr.feedback" pref is true.
      //  (and always unavailable when "extensions.dnr.enabled" pref is false)
      // testMatchOutcome_available: true - permission granted + pref true.
      // testMatchOutcome_available: false - no permission, pref doesn't matter.
      // testMatchOutcome_available: kTestMatchOutcomeNotAllowed - permission
      //   granted, but pref is false.
      testMatchOutcome_available = false,
    } = testExpectations;
    browser.test.assertEq(
      declarativeNetRequest_available,
      !!browser.declarativeNetRequest,
      "declarativeNetRequest API namespace availability"
    );

    // Dummy param for testMatchOutcome:
    const dummyRequest = { url: "https://example.com/", type: "other" };

    if (!testMatchOutcome_available) {
      browser.test.assertEq(
        undefined,
        browser.declarativeNetRequest?.testMatchOutcome,
        "declarativeNetRequest.testMatchOutcome availability"
      );
    } else if (testMatchOutcome_available === "kTestMatchOutcomeNotAllowed") {
      await browser.test.assertRejects(
        browser.declarativeNetRequest.testMatchOutcome(dummyRequest),
        `declarativeNetRequest.testMatchOutcome is only available when the "extensions.dnr.feedback" preference is set to true.`,
        "declarativeNetRequest.testMatchOutcome is unavailable"
      );
    } else {
      browser.test.assertDeepEq(
        { matchedRules: [] },
        await browser.declarativeNetRequest.testMatchOutcome(dummyRequest),
        "declarativeNetRequest.testMatchOutcome is available"
      );
    }
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
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

  // test_optional_declarativeNetRequestFeedback calls permission.request().
  // We don't care about the UI, only about the effect of being granted.
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );
});

add_task(
  {
    pref_set: [["extensions.dnr.enabled", false]],
  },
  async function extensions_dnr_enabled_pref_disabled_dnr_feature() {
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
          message:
            /Reading manifest: Invalid extension permission: declarativeNetRequest$/,
        },
        {
          message:
            /Reading manifest: Invalid extension permission: declarativeNetRequestFeedback/,
        },
        {
          message:
            /Reading manifest: Invalid extension permission: declarativeNetRequestWithHostAccess/,
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
        testMatchOutcome_available: kTestMatchOutcomeNotAllowed,
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
    forbidden: [
      {
        message:
          /Reading manifest: Invalid extension permission: declarativeNetRequest$/,
      },
      {
        message:
          /Reading manifest: Invalid extension permission: declarativeNetRequestFeedback/,
      },
      {
        message:
          /Reading manifest: Invalid extension permission: declarativeNetRequestWithHostAccess/,
      },
    ],
  });
});

add_task(async function dnr_available_in_mv2() {
  await testAvailability({
    allowDNRFeedback: true,
    testExpectations: {
      declarativeNetRequest_available: true,
      testMatchOutcome_available: true,
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
      testMatchOutcome_available: kTestMatchOutcomeNotAllowed,
    },
    manifest: {
      permissions: ["declarativeNetRequest", "declarativeNetRequestFeedback"],
    },
  });
});

add_task(
  { pref_set: [["extensions.dnr.feedback", true]] },
  async function declarativeNetRequestFeedback_is_optional() {
    async function background() {
      async function assertTestMatchOutcomeEnabled(expected, description) {
        let enabled;
        try {
          // testAvailability already checks the errors etc, so here we only
          // care about the method working vs not working.
          await browser.declarativeNetRequest.testMatchOutcome({
            url: "https://example.com/",
            type: "other",
          });
          enabled = true;
        } catch (e) {
          enabled = false;
        }
        browser.test.assertEq(expected, enabled, description);
      }

      await assertTestMatchOutcomeEnabled(false, "disabled when not granted");

      await new Promise(resolve => {
        // browser.test.withHandlingUserInput would have been simpler, but due
        // to bug 1598804 it cannot be used.
        browser.test.onMessage.addListener(async msg => {
          browser.test.assertEq("withHandlingUserInput_ok", msg, "Resuming");
          await browser.permissions.request({
            permissions: ["declarativeNetRequestFeedback"],
          });
          browser.test.sendMessage("withHandlingUserInput_done");
          resolve();
        });
        browser.test.sendMessage("withHandlingUserInput_wanted");
      });

      await assertTestMatchOutcomeEnabled(true, "enabled by permission");

      await browser.permissions.remove({
        permissions: ["declarativeNetRequestFeedback"],
      });
      await assertTestMatchOutcomeEnabled(false, "disabled after perm removal");

      browser.test.sendMessage("done");
    }
    let extension = ExtensionTestUtils.loadExtension({
      background,
      manifest: {
        manifest_version: 3,
        permissions: ["declarativeNetRequestWithHostAccess"],
        optional_permissions: ["declarativeNetRequestFeedback"],
      },
    });
    await extension.startup();
    await extension.awaitMessage("withHandlingUserInput_wanted");
    await withHandlingUserInput(extension, async () => {
      extension.sendMessage("withHandlingUserInput_ok");
      await extension.awaitMessage("withHandlingUserInput_done");
    });
    await extension.awaitMessage("done");
    await extension.unload();
  }
);

add_task(async function test_dnr_limits_namespace_properties() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
    },
    background() {
      browser.test.assertEq(
        "_dynamic",
        browser.declarativeNetRequest.DYNAMIC_RULESET_ID,
        "Value of DYNAMIC_RULESET_ID constant"
      );
      browser.test.assertEq(
        "_session",
        browser.declarativeNetRequest.SESSION_RULESET_ID,
        "Value of SESSION_RULESET_ID constant"
      );
      const {
        GUARANTEED_MINIMUM_STATIC_RULES,
        MAX_NUMBER_OF_STATIC_RULESETS,
        MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
        MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES,
        MAX_NUMBER_OF_REGEX_RULES,
      } = browser.declarativeNetRequest;
      browser.test.sendMessage("dnr-namespace-properties", {
        GUARANTEED_MINIMUM_STATIC_RULES,
        MAX_NUMBER_OF_STATIC_RULESETS,
        MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
        MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES,
        MAX_NUMBER_OF_REGEX_RULES,
      });
    },
  });

  await extension.startup();

  Assert.deepEqual(
    await extension.awaitMessage("dnr-namespace-properties"),
    ExtensionDNRLimits,
    "Got the expected limits values set on the dnr namespace"
  );

  await extension.unload();
});
