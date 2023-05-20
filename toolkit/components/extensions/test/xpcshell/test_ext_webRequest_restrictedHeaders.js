/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

const server = createHttpServer({
  hosts: ["example.net"],
});
server.registerPathHandler("/test/response-header", (req, res) => {
  let headerName;
  let headerValue;
  if (req.queryString) {
    let params = new URLSearchParams(req.queryString);
    headerName = params.get("name");
    headerValue = params.get("value");
    res.setHeader(headerName, headerValue, false);
    res.setHeader("test", `${headerName}=${headerValue}`, false);
  }
  res.write("");
});

const extensionData = {
  useAddonManager: "temporary",
  background() {
    const { manifest_version } = browser.runtime.getManifest();
    let headerToSet = undefined;
    browser.test.onMessage.addListener(function (msg, arg) {
      if (msg !== "header-to-set") {
        return;
      }
      headerToSet = arg;
      browser.test.sendMessage("header-to-set:done");
    });
    browser.webRequest.onHeadersReceived.addListener(
      e => {
        browser.test.log(`onHeadersReceived ${e.requestId} ${e.url}`);
        if (headerToSet === undefined) {
          browser.test.fail(
            "extension called before headerToSet option was set"
          );
        }
        if (typeof headerToSet?.name == "string") {
          const existingHeader = e.responseHeaders.filter(
            i => i.name.toLowerCase() === headerToSet.name
          )[0];
          e.responseHeaders = e.responseHeaders.filter(
            i => i.name.toLowerCase() != headerToSet.name
          );
          // Omit the header if the value isn't set, change the header otherwise.
          if (headerToSet.value != null) {
            e.responseHeaders.push({
              name: headerToSet.name,
              value: headerToSet.value,
            });
          }
          browser.test.log(
            `Test Extension MV${manifest_version} (${browser.runtime.id}) sets responseHeader: "${headerToSet.name}"="${headerToSet.value}" (was originally set to "${existingHeader?.value})"`
          );
        }
        return { responseHeaders: e.responseHeaders };
      },
      { urls: ["*://example.net/test/*"] },
      ["blocking", "responseHeaders"]
    );
    browser.webRequest.onCompleted.addListener(
      e => {
        browser.test.log(`onCompletedReceived ${e.requestId} ${e.url}`);
        const responseHeaders = e.responseHeaders.filter(
          i => i.name.toLowerCase() === headerToSet.name
        );

        browser.test.sendMessage(
          "on-completed:response-headers",
          responseHeaders
        );
      },
      { urls: ["*://example.net/test/*"] },
      ["responseHeaders"]
    );
    browser.test.sendMessage("bgpage:ready");
  },
};

const extDataMV2 = {
  ...extensionData,
  manifest: {
    manifest_version: 2,
    permissions: ["webRequest", "webRequestBlocking", "*://example.net/test/*"],
  },
};

const extDataMV3 = {
  ...extensionData,
  manifest: {
    manifest_version: 3,
    permissions: ["webRequest", "webRequestBlocking"],
    host_permissions: ["*://example.net/test/*"],
    granted_host_permissions: true,
  },
};

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

async function test_restricted_response_headers_changes({
  firstExtData,
  secondExtData,
  headerName,
  firstExtHeaderChange,
  secondExtHeaderChange,
  siteHeaderValue,
  expectedHeaderValue,
}) {
  const ext1 = ExtensionTestUtils.loadExtension(firstExtData);
  const ext2 = secondExtData && ExtensionTestUtils.loadExtension(secondExtData);

  await ext1.startup();
  await ext1.awaitMessage("bgpage:ready");

  await ext2?.startup();
  await ext2?.awaitMessage("bgpage:ready");

  ext1.sendMessage("header-to-set", {
    name: headerName,
    value: firstExtHeaderChange,
  });
  await ext1.awaitMessage("header-to-set:done");
  ext2?.sendMessage("header-to-set", {
    name: headerName,
    value: secondExtHeaderChange,
  });
  await ext2?.awaitMessage("header-to-set:done");

  if (siteHeaderValue) {
    await ExtensionTestUtils.fetch(
      "http://example.net/",
      `http://example.net/test/response-header?name=${headerName}&value=${siteHeaderValue}`
    );
  } else {
    await ExtensionTestUtils.fetch(
      "http://example.net/",
      "http://example.net/test/response-header"
    );
  }

  const [finalSiteHeaders] = await Promise.all([
    ext1.awaitMessage("on-completed:response-headers"),
    ext2?.awaitMessage("on-completed:response-headers"),
  ]);

  Assert.deepEqual(
    finalSiteHeaders,
    expectedHeaderValue
      ? [{ name: headerName, value: expectedHeaderValue }]
      : [],
    "Got the expected response header"
  );

  await ext1.unload();
  await ext2?.unload();
}

add_task(async function test_changes_to_restricted_response_headers() {
  const testCases = [
    {
      headerName: "cross-origin-embedder-policy",
      siteHeaderValue: "require-corp",
      firstExtHeaderChange: "credentialless",
      secondExtHeaderChange: "unsafe-none",
    },
    {
      headerName: "cross-origin-opener-policy",
      siteHeaderValue: "same-origin",
      firstExtHeaderChange: "same-origin-allow-popups",
      secondExtHeaderChange: "unsafe-none",
    },
    {
      headerName: "cross-origin-resource-policy",
      siteHeaderValue: "same-origin",
      firstExtHeaderChange: "same-site",
      secondExtHeaderChange: "cross-origin",
    },
    {
      headerName: "x-frame-options",
      siteHeaderValue: "deny",
      firstExtHeaderChange: "sameorigin",
      secondExtHeaderChange: "allow-from=http://example.com",
    },
    {
      headerName: "access-control-allow-credentials",
      siteHeaderValue: "true",
      firstExtHeaderChange: "false",
      secondExtHeaderChange: "false",
    },
    {
      headerName: "access-control-allow-methods",
      siteHeaderValue: "*",
      firstExtHeaderChange: "",
      secondExtHeaderChange: "GET",
    },
  ];

  for (const testCase of testCases) {
    info(
      `Test MV3 extension disallowed to change restricted header if already set by the website: "${testCase.headerName}"="${testCase.siteHeaderValue}`
    );
    await test_restricted_response_headers_changes({
      ...testCase,
      firstExtData: extDataMV3,
      // Expect the value set by the server to be preserved.
      expectedHeaderValue: testCase.siteHeaderValue,
    });
  }

  for (const testCase of testCases) {
    info(
      `Test MV3 extension disallowed to change restricted header also if not set by the website: "${testCase.headerName}`
    );
    await test_restricted_response_headers_changes({
      ...testCase,
      siteHeaderValue: null,
      firstExtData: extDataMV3,
      // Expect the value set by the server to be preserved.
      expectedHeaderValue: null,
    });
  }

  for (const testCase of testCases) {
    info(
      `Test MV2 extension allowed to change restricted header if already set by the website: ${JSON.stringify(
        testCase.siteHeader
      )}`
    );
    await test_restricted_response_headers_changes({
      ...testCase,
      firstExtData: extDataMV3,
      secondExtData: extDataMV2,
      // Expect the value set by the server to be preserved.
      expectedHeaderValue: testCase.secondExtHeaderChange,
    });
  }
});
