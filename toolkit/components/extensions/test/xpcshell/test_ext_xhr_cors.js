"use strict";

// The purpose of this test is to show that the XMLHttpRequest API behaves
// similarly in MV2 and MV3, except for intentional differences related to
// permission handling.

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({
  hosts: ["example.com", "example.net", "example.org"],
});
server.registerPathHandler("/dummy", (req, res) => {
  res.setStatusLine(req.httpVersion, 200, "OK");
  res.setHeader("Content-Type", "text/html; charset=utf-8");

  // A very strict CSP.
  res.setHeader(
    "Content-Security-Policy",
    "default-src; script-src 'nonce-kindasecret'; connect-src http:"
  );

  res.write(
    `<script id="id_of_some_element" nonce="kindasecret">
      // Clobber XMLHttpRequest API to allow us to verify that the page's value
      // for it does not affect the XMLHttpRequest API in the content script.
      window.XMLHttpRequest = "This is not XMLHttpRequest";
      </script>
    `
  );
});
server.registerPathHandler("/dummy.json", (req, res) => {
  res.write(`{"mykey": "kvalue"}`);
});
server.registerPathHandler("/nocors", (req, res) => {
  res.write("no cors");
});
server.registerPathHandler("/cors-enabled", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "http://example.com");
  res.write("cors_response");
});

// We just need to test XHR; fetch is already covered by test_ext_secfetch.js.
async function test_xhr({ manifest_version }) {
  async function contentScript(manifest_version) {
    function runXHR(url, extraXHRProps) {
      return new Promise(resolve => {
        let x = new XMLHttpRequest();
        x.open("GET", url);
        Object.assign(x, extraXHRProps);
        x.onloadend = () => resolve(x);
        x.send();
      });
    }
    async function checkXHR({ description, url, extraXHRProps, expected }) {
      let { status, response } = expected;
      let x = await runXHR(url, extraXHRProps);
      browser.test.assertEq(status, x.status, `${description} - status`);
      browser.test.assertEq(response, x.response, `${description} - body`);
    }

    await checkXHR({
      description: "Same-origin",
      url: "http://example.com/nocors",
      expected: { status: 200, response: "no cors" },
    });

    await checkXHR({
      description: "Cross-origin without CORS",
      url: "http://example.org/nocors",
      expected: { status: 0, response: "" },
    });

    await checkXHR({
      description: "Cross-origin with CORS",
      url: "http://example.org/cors-enabled",
      expected:
        manifest_version === 2
          ? // Bug 1605197: MV2 cannot fall back to CORS.
            { status: 0, response: "" }
          : { status: 200, response: "cors_response" },
    });

    // MV2 allowed cross-origin requests in content scripts with host
    // permissions, but MV3 does not.
    await checkXHR({
      description: "Cross-origin without CORS, with permission",
      url: "http://example.net/nocors",
      expected:
        manifest_version === 2
          ? { status: 200, response: "no cors" }
          : { status: 0, response: "" },
    });

    await checkXHR({
      description: "Cross-origin with CORS (and permission)",
      url: "http://example.net/cors-enabled",
      expected: { status: 200, response: "cors_response" },
    });

    // MV2 has a XMLHttpRequest instance specific to the sandbox.
    // MV3 uses the page's XMLHttpRequest and currently enforces the page's CSP.
    // TODO bug 1766813: Enforce content script CSP instead.
    await checkXHR({
      description: "data:-URL while page blocks data: via CSP",
      url: "data:,data-url",
      expected:
        // Should be "data-url" in MV3 too.
        manifest_version === 2
          ? { status: 200, response: "data-url" }
          : { status: 0, response: "" },
    });

    {
      let x = await runXHR("http://example.com/dummy.json", {
        responseType: "json",
      });
      browser.test.assertTrue(x.response instanceof Object, "is JSON object");
      browser.test.assertEq(x.response.mykey, "kvalue", "can read parsed JSON");
    }

    {
      let x = await runXHR("http://example.com/dummy", {
        responseType: "document",
      });
      browser.test.assertTrue(x.response instanceof HTMLDocument, "is doc");
      browser.test.assertTrue(
        x.response.querySelector("#id_of_some_element"),
        "got parsed document"
      );
    }
    browser.test.sendMessage("done");
  }
  let extension = ExtensionTestUtils.loadExtension({
    temporarilyInstalled: true, // Needed for granted_host_permissions
    manifest: {
      manifest_version,
      granted_host_permissions: true, // Test-only: grant permissions in MV3.
      host_permissions: [
        "http://example.net/",
        // Work-around for bug 1766752.
        "http://example.com/",
        // "http://example.org/" is intentionally missing.
      ],
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          run_at: "document_end",
          js: ["contentscript.js"],
        },
      ],
    },
    files: {
      "contentscript.js": `(${contentScript})(${manifest_version})`,
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitMessage("done");
  await contentPage.close();

  await extension.unload();
}

add_task(async function test_XHR_MV2() {
  await test_xhr({ manifest_version: 2 });
});

add_task(async function test_XHR_MV3() {
  await test_xhr({ manifest_version: 3 });
});
