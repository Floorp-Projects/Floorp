/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionAPI } = ExtensionCommon;

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

// Because the `mockExtensionAPI` is currently the only "mock" API that has
// WebIDL bindings, this is the only namespace we can use in our tests. There
// is no JSON schema for this namespace so we add one here that is tailored for
// our testing needs.
const API = class extends ExtensionAPI {
  getAPI(context) {
    return {
      mockExtensionAPI: {
        methodAsync: files => {
          return files;
        },
      },
    };
  }
};

const SCHEMA = [
  {
    namespace: "mockExtensionAPI",
    functions: [
      {
        name: "methodAsync",
        type: "function",
        async: true,
        parameters: [
          {
            name: "files",
            type: "array",
            items: { $ref: "manifest.ExtensionURL" },
          },
        ],
      },
    ],
  },
];

add_setup(async function() {
  await AddonTestUtils.promiseStartupManager();

  // The blob:-URL registered in `registerModules()` below gets loaded at:
  // https://searchfox.org/mozilla-central/rev/0fec57c05d3996cc00c55a66f20dd5793a9bfb5d/toolkit/components/extensions/ExtensionCommon.jsm#1649
  Services.prefs.setBoolPref(
    "security.allow_parent_unrestricted_js_loads",
    true
  );

  ExtensionParent.apiManager.registerModules({
    mockExtensionAPI: {
      schema: `data:,${JSON.stringify(SCHEMA)}`,
      scopes: ["addon_parent"],
      paths: [["mockExtensionAPI"]],
      url: URL.createObjectURL(
        new Blob([`this.mockExtensionAPI = ${API.toString()}`])
      ),
    },
  });
});

add_task(async function test_relative_urls() {
  await runExtensionAPITest(
    "should format arguments with the relativeUrl formatter",
    {
      backgroundScript() {
        return browser.mockExtensionAPI.methodAsync([
          "script-1.js",
          "script-2.js",
        ]);
      },
      mockAPIRequestHandler(policy, request) {
        return this._handleAPIRequest_orig(policy, request);
      },
      assertResults({ testResult, testError, extension }) {
        Assert.deepEqual(
          testResult,
          [
            `moz-extension://${extension.uuid}/script-1.js`,
            `moz-extension://${extension.uuid}/script-2.js`,
          ],
          "expected correct url"
        );
        Assert.deepEqual(testError, undefined, "expected no error");
      },
    }
  );
});
