"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_contentscript_exportHelpers() {
  function contentScript() {
    browser.test.assertTrue(typeof cloneInto === "function");
    browser.test.assertTrue(typeof createObjectIn === "function");
    browser.test.assertTrue(typeof exportFunction === "function");

    /* globals exportFunction, precisePi, reportPi */
    let value = 3.14;
    exportFunction(() => value, window, { defineAs: "precisePi" });

    browser.test.assertEq(
      "undefined",
      typeof precisePi,
      "exportFunction should export to the page's scope only"
    );

    browser.test.assertEq(
      "undefined",
      typeof window.precisePi,
      "exportFunction should export to the page's scope only"
    );

    let results = [];
    exportFunction(pi => results.push(pi), window, { defineAs: "reportPi" });

    let s = document.createElement("script");
    s.textContent = `(${function() {
      let result1 = "unknown 1";
      let result2 = "unknown 2";
      try {
        result1 = precisePi();
      } catch (e) {
        result1 = "err:" + e;
      }
      try {
        result2 = window.precisePi();
      } catch (e) {
        result2 = "err:" + e;
      }
      reportPi(result1);
      reportPi(result2);
    }})();`;

    document.documentElement.appendChild(s);
    // Inline script ought to run synchronously.

    browser.test.assertEq(
      3.14,
      results[0],
      "exportFunction on window should define a global function"
    );
    browser.test.assertEq(
      3.14,
      results[1],
      "exportFunction on window should export a property to window."
    );

    browser.test.assertEq(
      2,
      results.length,
      "Expecting the number of results to match the number of method calls"
    );

    browser.test.notifyPass("export helper test completed");
  }

  let extensionData = {
    manifest: {
      content_scripts: [
        {
          js: ["contentscript.js"],
          matches: ["http://example.com/data/file_sample.html"],
          run_at: "document_start",
        },
      ],
    },

    files: {
      "contentscript.js": contentScript,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitFinish("export helper test completed");
  await contentPage.close();
  await extension.unload();
});
