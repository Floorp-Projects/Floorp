"use strict";

/* globals browser */
const { AddonSettings } = ChromeUtils.import(
  "resource://gre/modules/addons/AddonSettings.jsm"
);

AddonTestUtils.init(this);

add_task(async function setup() {
  AddonTestUtils.overrideCertDB();
  await ExtensionTestUtils.startAddonManager();
});

let fooExperimentAPIs = {
  foo: {
    schema: "schema.json",
    parent: {
      scopes: ["addon_parent"],
      script: "parent.js",
      paths: [["experiments", "foo", "parent"]],
    },
    child: {
      scopes: ["addon_child"],
      script: "child.js",
      paths: [["experiments", "foo", "child"]],
    },
  },
};

let fooExperimentFiles = {
  "schema.json": JSON.stringify([
    {
      namespace: "experiments.foo",
      types: [
        {
          id: "Meh",
          type: "object",
          properties: {},
        },
      ],
      functions: [
        {
          name: "parent",
          type: "function",
          async: true,
          parameters: [],
        },
        {
          name: "child",
          type: "function",
          parameters: [],
          returns: { type: "string" },
        },
      ],
    },
  ]),

  /* globals ExtensionAPI */
  "parent.js": () => {
    this.foo = class extends ExtensionAPI {
      getAPI(context) {
        return {
          experiments: {
            foo: {
              parent() {
                return Promise.resolve("parent");
              },
            },
          },
        };
      }
    };
  },

  "child.js": () => {
    this.foo = class extends ExtensionAPI {
      getAPI(context) {
        return {
          experiments: {
            foo: {
              child() {
                return "child";
              },
            },
          },
        };
      }
    };
  },
};

async function testFooExperiment() {
  browser.test.assertEq(
    "object",
    typeof browser.experiments,
    "typeof browser.experiments"
  );

  browser.test.assertEq(
    "object",
    typeof browser.experiments.foo,
    "typeof browser.experiments.foo"
  );

  browser.test.assertEq(
    "function",
    typeof browser.experiments.foo.child,
    "typeof browser.experiments.foo.child"
  );

  browser.test.assertEq(
    "function",
    typeof browser.experiments.foo.parent,
    "typeof browser.experiments.foo.parent"
  );

  browser.test.assertEq(
    "child",
    browser.experiments.foo.child(),
    "foo.child()"
  );

  browser.test.assertEq(
    "parent",
    await browser.experiments.foo.parent(),
    "await foo.parent()"
  );
}

async function testFooFailExperiment() {
  browser.test.assertEq(
    "object",
    typeof browser.experiments,
    "typeof browser.experiments"
  );

  browser.test.assertEq(
    "undefined",
    typeof browser.experiments.foo,
    "typeof browser.experiments.foo"
  );
}

add_task(async function test_bundled_experiments() {
  let testCases = [
    { isSystem: true, temporarilyInstalled: true, shouldHaveExperiments: true },
    {
      isSystem: true,
      temporarilyInstalled: false,
      shouldHaveExperiments: true,
    },
    {
      isPrivileged: true,
      temporarilyInstalled: true,
      shouldHaveExperiments: true,
    },
    {
      isPrivileged: true,
      temporarilyInstalled: false,
      shouldHaveExperiments: true,
    },
    {
      isPrivileged: false,
      temporarilyInstalled: true,
      shouldHaveExperiments: AddonSettings.EXPERIMENTS_ENABLED,
    },
    {
      isPrivileged: false,
      temporarilyInstalled: false,
      shouldHaveExperiments: AppConstants.MOZ_APP_NAME == "thunderbird",
    },
  ];

  async function background(shouldHaveExperiments) {
    if (shouldHaveExperiments) {
      await testFooExperiment();
    } else {
      await testFooFailExperiment();
    }

    browser.test.notifyPass("background.experiments.foo");
  }

  for (let testCase of testCases) {
    let extension = ExtensionTestUtils.loadExtension({
      isPrivileged: testCase.isPrivileged,
      isSystem: testCase.isSystem,
      temporarilyInstalled: testCase.temporarilyInstalled,

      manifest: {
        experiment_apis: fooExperimentAPIs,
      },

      background: `
        ${testFooExperiment}
        ${testFooFailExperiment}
        (${background})(${testCase.shouldHaveExperiments});
      `,

      files: fooExperimentFiles,
    });

    await extension.startup();

    await extension.awaitFinish("background.experiments.foo");

    await extension.unload();
  }
});

add_task(async function test_unbundled_experiments() {
  async function background() {
    await testFooExperiment();

    browser.test.assertEq(
      "object",
      typeof browser.experiments.crunk,
      "typeof browser.experiments.crunk"
    );

    browser.test.assertEq(
      "function",
      typeof browser.experiments.crunk.child,
      "typeof browser.experiments.crunk.child"
    );

    browser.test.assertEq(
      "function",
      typeof browser.experiments.crunk.parent,
      "typeof browser.experiments.crunk.parent"
    );

    browser.test.assertEq(
      "crunk-child",
      browser.experiments.crunk.child(),
      "crunk.child()"
    );

    browser.test.assertEq(
      "crunk-parent",
      await browser.experiments.crunk.parent(),
      "await crunk.parent()"
    );

    browser.test.notifyPass("background.experiments.crunk");
  }

  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,

    manifest: {
      experiment_apis: fooExperimentAPIs,

      permissions: ["experiments.crunk"],
    },

    background: `
      ${testFooExperiment}
      (${background})();
    `,

    files: fooExperimentFiles,
  });

  let apiExtension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,

    manifest: {
      applications: { gecko: { id: "crunk@experiments.addons.mozilla.org" } },

      experiment_apis: {
        crunk: {
          schema: "schema.json",
          parent: {
            scopes: ["addon_parent"],
            script: "parent.js",
            paths: [["experiments", "crunk", "parent"]],
          },
          child: {
            scopes: ["addon_child"],
            script: "child.js",
            paths: [["experiments", "crunk", "child"]],
          },
        },
      },
    },

    files: {
      "schema.json": JSON.stringify([
        {
          namespace: "experiments.crunk",
          types: [
            {
              id: "Meh",
              type: "object",
              properties: {},
            },
          ],
          functions: [
            {
              name: "parent",
              type: "function",
              async: true,
              parameters: [],
            },
            {
              name: "child",
              type: "function",
              parameters: [],
              returns: { type: "string" },
            },
          ],
        },
      ]),

      "parent.js": () => {
        this.crunk = class extends ExtensionAPI {
          getAPI(context) {
            return {
              experiments: {
                crunk: {
                  parent() {
                    return Promise.resolve("crunk-parent");
                  },
                },
              },
            };
          }
        };
      },

      "child.js": () => {
        this.crunk = class extends ExtensionAPI {
          getAPI(context) {
            return {
              experiments: {
                crunk: {
                  child() {
                    return "crunk-child";
                  },
                },
              },
            };
          }
        };
      },
    },
  });

  await apiExtension.startup();
  await extension.startup();

  await extension.awaitFinish("background.experiments.crunk");

  await extension.unload();
  await apiExtension.unload();
});
