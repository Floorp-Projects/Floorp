"use strict";

let gleanAPIs = {
  gleanHelper: {
    schema: "schema.json",
    parent: {
      scopes: ["addon_parent"],
      script: "parent.js",
      paths: [["gleanHelper"]],
    },
    child: {
      scopes: ["addon_child"],
      script: "child.js",
      paths: [["gleanHelper"]],
    },
  },
};

let gleanFiles = {
  "schema.json": JSON.stringify([
    {
      namespace: "gleanHelper",
      functions: [
        {
          name: "testGleanTelemetryChild",
          type: "function",
          parameters: [],
        },
        {
          name: "submitTestPingChild",
          type: "function",
          parameters: [],
        },
        {
          name: "testGleanTelemetryParent",
          type: "function",
          parameters: [],
          async: true,
        },
        {
          name: "submitTestPingParent",
          type: "function",
          parameters: [],
          async: true,
        },
      ],
    },
  ]),
  "parent.js": () => {
    this.gleanHelper = class extends ExtensionAPI {
      getAPI() {
        return {
          gleanHelper: {
            async testGleanTelemetryParent() {
              Glean.testOnly.badCode.add(42);
            },
            async submitTestPingParent() {
              GleanPings.testPing.submit();
            },
          },
        };
      }
    };
  },
  "child.js": () => {
    this.gleanHelper = class extends ExtensionAPI {
      getAPI() {
        return {
          gleanHelper: {
            testGleanTelemetryChild() {
              Glean.testOnly.badCode.add(64);
            },
            submitTestPingChild() {
              GleanPings.testPing.submit();
            },
          },
        };
      }
    };
  },
};

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

// Verify access to Glean and GleanPings from extension child/parent.
add_task(async function test_glean_access_in_extension_child() {
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      experiment_apis: gleanAPIs,
    },
    files: gleanFiles,
    async background() {
      browser.gleanHelper.testGleanTelemetryChild();
      browser.gleanHelper.submitTestPingChild();
      await browser.gleanHelper.testGleanTelemetryParent();
      await browser.gleanHelper.submitTestPingParent();
      browser.test.sendMessage("done");
    },
  });

  // Setup `testBeforeNextSubmit` callback.
  let testPingSubmitted = 0;
  GleanPings.testPing.testBeforeNextSubmit(() => {
    Assert.equal(64, Glean.testOnly.badCode.testGetValue());
    Assert.equal(
      0,
      testPingSubmitted,
      "Test ping was submitted, callback was called from child."
    );
    testPingSubmitted += 1;

    // Set up the callback again, this time to be invoked by the parent.
    GleanPings.testPing.testBeforeNextSubmit(() => {
      Assert.equal(42, Glean.testOnly.badCode.testGetValue());
      Assert.equal(
        1,
        testPingSubmitted,
        "Test ping was submitted, callback was called from parent."
      );
      testPingSubmitted += 1;
    });
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();

  // Final check to ensure both callbacks were invoked
  Assert.equal(
    2,
    testPingSubmitted,
    "Test pings was submitted, callback was called from parent & child."
  );

  // Cleanup
  Services.fog.testResetFOG();
});
