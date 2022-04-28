"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(
    `<script>
      // Clobber the JSON API to allow us to confirm that the page's value for
      // the "JSON" object does not affect the content script's JSON API.
      window.JSON = new String("overridden by page");
      window.objFromPage = { serializeMe: "thanks" };
      window.objWithToJSON = { toJSON: () => "toJSON ran", should_not_see: 1 };
      </script>
    `
  );
});

async function test_JSON_parse_and_stringify({ manifest_version }) {
  let extension = ExtensionTestUtils.loadExtension({
    temporarilyInstalled: true, // Needed for granted_host_permissions
    manifest: {
      manifest_version,
      granted_host_permissions: true, // Test-only: grant permissions in MV3.
      host_permissions: ["http://example.com/"], // Work-around for bug 1766752.
      content_scripts: [
        {
          matches: ["http://example.com/dummy"],
          run_at: "document_end",
          js: ["contentscript.js"],
        },
      ],
    },
    files: {
      "contentscript.js"() {
        let json = `{"a":[123,true,null]}`;
        browser.test.assertEq(
          JSON.stringify({ a: [123, true, null] }),
          json,
          "JSON.stringify with basic values"
        );
        let parsed = JSON.parse(json);
        browser.test.assertTrue(
          parsed instanceof Object,
          "Parsed JSON is an Object"
        );
        browser.test.assertTrue(
          parsed.a instanceof Array,
          "Parsed JSON has an Array"
        );
        browser.test.assertEq(
          JSON.stringify(parsed),
          json,
          "JSON.stringify for parsed JSON returns original input"
        );
        browser.test.assertEq(
          JSON.stringify({ toJSON: () => "overridden", hideme: true }),
          `"overridden"`,
          "JSON.parse with toJSON method"
        );

        browser.test.assertEq(
          JSON.stringify(window.wrappedJSObject.objFromPage),
          `{"serializeMe":"thanks"}`,
          "JSON.parse with value from the page"
        );

        browser.test.assertEq(
          JSON.stringify(window.wrappedJSObject.objWithToJSON),
          `"toJSON ran"`,
          "JSON.parse with object with toJSON method from the page"
        );

        browser.test.assertTrue(JSON === globalThis.JSON, "JSON === this.JSON");
        browser.test.assertTrue(JSON === window.JSON, "JSON === window.JSON");
        browser.test.assertEq(
          "overridden by page",
          window.wrappedJSObject.JSON.toString(),
          "page's JSON object is still the original value (overridden by page)"
        );
        browser.test.sendMessage("done");
      },
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

add_task(async function test_JSON_apis_MV2() {
  await test_JSON_parse_and_stringify({ manifest_version: 2 });
});

add_task(async function test_JSON_apis_MV3() {
  await test_JSON_parse_and_stringify({ manifest_version: 3 });
});
