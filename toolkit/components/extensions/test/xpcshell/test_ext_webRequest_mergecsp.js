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
  hosts: ["example.net", "example.com"],
});
server.registerDirectory("/data/", do_get_file("data"));

const pageContent = `<!DOCTYPE html>
  <script id="script1" src="/data/file_script_good.js"></script>
  <script id="script3" src="//example.com/data/file_script_bad.js"></script>
  <img id="img1" src='/data/file_image_good.png'>
  <img id="img3" src='//example.com/data/file_image_good.png'>
`;

server.registerPathHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html");
  if (request.queryString) {
    response.setHeader(
      "Content-Security-Policy",
      decodeURIComponent(request.queryString)
    );
  }
  response.write(pageContent);
});

let extensionData = {
  manifest: {
    permissions: ["webRequest", "webRequestBlocking", "*://example.net/*"],
  },
  background() {
    let csp_value = undefined;
    browser.test.onMessage.addListener(function(msg) {
      csp_value = msg;
      browser.test.sendMessage("csp-set");
    });
    browser.webRequest.onHeadersReceived.addListener(
      e => {
        browser.test.log(`onHeadersReceived ${e.requestId} ${e.url}`);
        if (csp_value === undefined) {
          browser.test.assertTrue(false, "extension called before CSP was set");
        }
        if (csp_value !== null) {
          e.responseHeaders = e.responseHeaders.filter(
            i => i.name.toLowerCase() != "content-security-policy"
          );
          if (csp_value !== "") {
            e.responseHeaders.push({
              name: "Content-Security-Policy",
              value: csp_value,
            });
          }
        }
        return { responseHeaders: e.responseHeaders };
      },
      { urls: ["*://example.net/*"] },
      ["blocking", "responseHeaders"]
    );
  },
};

/**
 * Test a combination of Content Security Policies against first/third party images/scripts.
 * @param {Object} opts
 * @param {string} opts.site_csp The CSP to be sent by the site, or null.
 * @param {string} opts.ext1_csp The CSP to be sent by the first extension,
 *                          "" to remove the header, or null to not modify it.
 * @param {string} opts.ext2_csp The CSP to be sent by the first extension,
 *                          "" to remove the header, or null to not modify it.
 * @param {Object} opts.expect   Object containing information which resources are expected to be loaded.
 * @param {Object} opts.expect.img1_loaded    image from a first party origin.
 * @param {Object} opts.expect.img3_loaded    image from a third party origin.
 * @param {Object} opts.expect.script1_loaded script from a first party origin.
 * @param {Object} opts.expect.script3_loaded script from a third party origin.
 * @param {Object} [opts.expect.cspJSON] expected final document CSP (in JSON format, See dom/webidl/CSPDictionaries.webidl).
 * @param {Object} [opts.extData1] first test extension definition data (defaults to extensionData).
 * @param {Object} [opts.extData2] second test extension definition data (defaults to extensionData).
 */
async function test_csp({
  site_csp,
  ext1_csp,
  ext2_csp,
  expect,
  ext1_data = extensionData,
  ext2_data = extensionData,
}) {
  let extension1 = await ExtensionTestUtils.loadExtension(ext1_data);
  let extension2 = await ExtensionTestUtils.loadExtension(ext2_data);
  await extension1.startup();
  await extension2.startup();
  extension1.sendMessage(ext1_csp);
  extension2.sendMessage(ext2_csp);
  await extension1.awaitMessage("csp-set");
  await extension2.awaitMessage("csp-set");

  let csp_value = encodeURIComponent(site_csp || "");
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.net/?${csp_value}`
  );
  let results = await contentPage.spawn(null, async () => {
    let img1 = this.content.document.getElementById("img1");
    let img3 = this.content.document.getElementById("img3");
    let cspJSON = JSON.parse(this.content.document.cspJSON);
    return {
      img1_loaded: img1.complete && img1.naturalWidth > 0,
      img3_loaded: img3.complete && img3.naturalWidth > 0,
      // Note: "good" and "bad" are just placeholders; they don't mean anything.
      script1_loaded: !!this.content.document.getElementById("good"),
      script3_loaded: !!this.content.document.getElementById("bad"),
      cspJSON,
    };
  });

  await contentPage.close();
  await extension1.unload();
  await extension2.unload();

  let action = {
    true: "loaded",
    false: "blocked",
  };

  info(
    `test_csp: From "${site_csp}" to ${JSON.stringify(
      ext1_csp
    )} to ${JSON.stringify(ext2_csp)}`
  );

  equal(
    expect.img1_loaded,
    results.img1_loaded,
    `expected first party image to be ${action[expect.img1_loaded]}`
  );
  equal(
    expect.img3_loaded,
    results.img3_loaded,
    `expected third party image to be ${action[expect.img3_loaded]}`
  );
  equal(
    expect.script1_loaded,
    results.script1_loaded,
    `expected first party script to be ${action[expect.script1_loaded]}`
  );
  equal(
    expect.script3_loaded,
    results.script3_loaded,
    `expected third party script to be ${action[expect.script3_loaded]}`
  );

  if (expect.cspJSON) {
    Assert.deepEqual(
      expect.cspJSON,
      results.cspJSON["csp-policies"],
      `Got the expected final CSP set on the content document`
    );
  }
}

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

// Test that merging csp header on both mv2 and mv3 extensions
// (and combination of both).
add_task(async function test_webRequest_mergecsp() {
  const testCases = [
    {
      site_csp: "default-src *",
      ext1_csp: "script-src 'none'",
      ext2_csp: null,
      expect: {
        img1_loaded: true,
        img3_loaded: true,
        script1_loaded: false,
        script3_loaded: false,
      },
    },
    {
      site_csp: null,
      ext1_csp: "script-src 'none'",
      ext2_csp: null,
      expect: {
        img1_loaded: true,
        img3_loaded: true,
        script1_loaded: false,
        script3_loaded: false,
      },
    },
    {
      site_csp: "default-src *",
      ext1_csp: "script-src 'none'",
      ext2_csp: "img-src 'none'",
      expect: {
        img1_loaded: false,
        img3_loaded: false,
        script1_loaded: false,
        script3_loaded: false,
      },
    },
    {
      site_csp: null,
      ext1_csp: "script-src 'none'",
      ext2_csp: "img-src 'none'",
      expect: {
        img1_loaded: false,
        img3_loaded: false,
        script1_loaded: false,
        script3_loaded: false,
      },
    },
    {
      site_csp: "default-src *",
      ext1_csp: "img-src example.com",
      ext2_csp: "img-src example.org",
      expect: {
        img1_loaded: false,
        img3_loaded: false,
        script1_loaded: true,
        script3_loaded: true,
      },
    },
  ];

  const extMV2Data = { ...extensionData };
  const extMV3Data = {
    ...extensionData,
    useAddonManager: "temporary",
    manifest: {
      ...extensionData.manifest,
      manifest_version: 3,
      permissions: ["webRequest", "webRequestBlocking"],
      host_permissions: ["*://example.net/*"],
      granted_host_permissions: true,
    },
  };

  info("Run all test cases on ext1 MV2 and ext2 MV2");
  for (const testCase of testCases) {
    await test_csp({
      ...testCase,
      ext1_data: extMV2Data,
      ext2_data: extMV2Data,
    });
  }

  info("Run all test cases on ext1 MV3 and ext2 MV3");
  for (const testCase of testCases) {
    await test_csp({
      ...testCase,
      ext1_data: extMV3Data,
      ext2_data: extMV3Data,
    });
  }

  info("Run all test cases on ext1 MV3 and ext2 MV2");
  for (const testCase of testCases) {
    await test_csp({
      ...testCase,
      ext1_data: extMV3Data,
      ext2_data: extMV2Data,
    });
  }

  info("Run all test cases on ext1 MV2 and ext2 MV3");
  for (const testCase of testCases) {
    await test_csp({
      ...testCase,
      ext1_data: extMV2Data,
      ext2_data: extMV3Data,
    });
  }
});

add_task(async function test_remove_and_replace_csp_mv2() {
  // CSP removed, CSP added.
  await test_csp({
    site_csp: "img-src 'self'",
    ext1_csp: "",
    ext2_csp: "img-src example.com",
    expect: {
      img1_loaded: false,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
    },
  });

  // CSP removed, CSP added.
  await test_csp({
    site_csp: "default-src 'none'",
    ext1_csp: "",
    ext2_csp: "img-src example.com",
    expect: {
      img1_loaded: false,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
    },
  });

  // CSP replaced - regression test for bug 1635781.
  await test_csp({
    site_csp: "default-src 'none'",
    ext1_csp: "img-src example.com",
    ext2_csp: null,
    expect: {
      img1_loaded: false,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
    },
  });

  // CSP unchanged, CSP replaced - regression test for bug 1635781.
  await test_csp({
    site_csp: "default-src 'none'",
    ext1_csp: null,
    ext2_csp: "img-src example.com",
    expect: {
      img1_loaded: false,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
    },
  });

  // CSP replaced, CSP removed.
  await test_csp({
    site_csp: "default-src 'none'",
    ext1_csp: "img-src example.com",
    ext2_csp: "",
    expect: {
      img1_loaded: true,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
    },
  });
});

// Test that fully replace the website csp header from an mv3 extension
// isn't allowed and it is considered a no-op.
add_task(async function test_remove_and_replace_csp_mv3() {
  const extMV2Data = { ...extensionData };

  const extMV3Data = {
    ...extensionData,
    useAddonManager: "temporary",
    manifest: {
      ...extensionData.manifest,
      manifest_version: 3,
      permissions: ["webRequest", "webRequestBlocking"],
      host_permissions: ["*://example.net/*"],
      granted_host_permissions: true,
    },
  };

  await test_csp({
    // site: CSP strict on images, lax on default and script src.
    site_csp: "img-src 'self'",
    // ext1: MV3 extension which return an empty CSP header (which is a no-op).
    ext1_csp: "",
    // ext2: MV3 extension which return a CSP header (which is expected to be merged).
    ext2_csp: "img-src example.com",
    expect: {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: true,
      script3_loaded: true,
      cspJSON: [
        { "img-src": ["'self'"], "report-only": false },
        { "img-src": ["http://example.com"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension which return an empty CSP header (which is a no-op).
    ext1_csp: "",
    // ext2: MV3 extension which return a CSP header (which is expected to be merged).
    ext2_csp: "img-src example.com",
    expect: {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: false,
      script3_loaded: false,
      cspJSON: [
        { "default-src": ["'none'"], "report-only": false },
        { "img-src": ["http://example.com"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension which return a CSP header (which is expected to be merged and to
    // not be able to make it less strict).
    ext1_csp: "img-src example.com",
    // ext2: MV3 extension which leaves the header unmodified.
    ext2_csp: null,
    expect: {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: false,
      script3_loaded: false,
      cspJSON: [
        { "default-src": ["'none'"], "report-only": false },
        { "img-src": ["http://example.com"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension which merges additional directive into the site csp (and can't make
    // it less strict).
    ext1_csp: "img-src example.com",
    // ext2: MV3 extension which merges an empty CSP header (which is a no-op, unlike with MV2).
    ext2_csp: "",
    expect: {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: false,
      script3_loaded: false,
      cspJSON: [
        { "default-src": ["'none'"], "report-only": false },
        { "img-src": ["http://example.com"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: lax CSP (which is expected to be made stricted by the ext1 extension).
    site_csp: "default-src *",
    // ext1: MV3 extension which wants to set a stricter CSP (expected to work fine with the MV3 extension)
    ext1_csp: "default-src 'none'",
    // ext2: MV3 extension which leaves it unchanged.
    ext2_csp: null,
    expect: {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: false,
      script3_loaded: false,
      cspJSON: [
        { "default-src": ["*"], "report-only": false },
        { "default-src": ["'none'"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension and tries to replace the strict site csp with this lax one
    // (but as an MV3 extension that is going to be merged to the site csp and the
    // resulting site CSP is expected to stay strict).
    ext1_csp: "default-src *",
    // ext2: MV3 extension which leaves it unchanged.
    ext2_csp: null,
    expect: {
      // strict site csp merged with the lax one from ext1 stays strict.
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: false,
      script3_loaded: false,
      cspJSON: [
        { "default-src": ["'none'"], "report-only": false },
        { "default-src": ["*"], "report-only": false },
      ],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV3Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension which return an empty CSP (expected to be a no-op for an MV3 extension).
    ext1_csp: "",
    // ext2: MV2 exension which wants to replace the site csp with a lax one (and still be allowed to
    // because the empty one from the MV3 extension is expected to be a no-op).
    ext2_csp: "default-src *",
    expect: {
      img1_loaded: true,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
      cspJSON: [{ "default-src": ["*"], "report-only": false }],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV2Data,
  });

  await test_csp({
    // site: CSP strict on default-src.
    site_csp: "default-src 'none'",
    // ext1: MV3 extension which return an empty CSP (which is expected to be a no-op).
    ext1_csp: "",
    // ext2: MV2 extension which also returns an empty CSP (which for an MV2 extension is expected
    // to clear the CSP).
    ext2_csp: "",
    expect: {
      img1_loaded: true,
      img3_loaded: true,
      script1_loaded: true,
      script3_loaded: true,
      // Expect the resulting final document CSP to be empty (due to the MV2 extension clearing it).
      cspJSON: [],
    },
    ext1_data: extMV3Data,
    ext2_data: extMV2Data,
  });
});
