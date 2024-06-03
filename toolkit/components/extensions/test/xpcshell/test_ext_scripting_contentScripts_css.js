"use strict";

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

// ExtensionContent.sys.mjs needs to know when it's running from xpcshell, to use
// the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

const makeExtension = ({ manifest: manifestProps, ...otherProps }) => {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 3,
      permissions: ["scripting"],
      host_permissions: ["http://localhost/*"],
      granted_host_permissions: true,
      ...manifestProps,
    },
    allowInsecureRequests: true,
    temporarilyInstalled: true,
    ...otherProps,
  });
};

add_task(async function test_registerContentScripts_css() {
  let extension = makeExtension({
    async background() {
      // This script is injected in all frames after the styles so that we can
      // verify the registered styles.
      const checkAppliedStyleScript = {
        id: "check-applied-styles",
        allFrames: true,
        matches: ["http://*/*/*.html"],
        matchOriginAsFallback: false,
        runAt: "document_idle",
        world: "ISOLATED",
        persistAcrossSessions: false,
        js: ["check-applied-styles.js"],
      };

      // Listen to the `load-test-case` message and unregister/register new
      // content scripts.
      browser.test.onMessage.addListener(async (msg, data) => {
        switch (msg) {
          case "load-test-case":
            const { title, params, skipCheckScriptRegistration } = data;
            const expectedScripts = [];

            await browser.scripting.unregisterContentScripts();

            if (!skipCheckScriptRegistration) {
              await browser.scripting.registerContentScripts([
                checkAppliedStyleScript,
              ]);

              expectedScripts.push(checkAppliedStyleScript);
            }

            expectedScripts.push(...params);

            const res = await browser.scripting.registerContentScripts(params);
            browser.test.assertEq(
              res,
              undefined,
              `${title} - expected no result`
            );
            const scripts =
              await browser.scripting.getRegisteredContentScripts();
            browser.test.assertEq(
              expectedScripts.length,
              scripts.length,
              `${title} - expected ${expectedScripts.length} registered scripts`
            );
            browser.test.assertEq(
              JSON.stringify(expectedScripts),
              JSON.stringify(scripts),
              `${title} - got expected registered scripts`
            );

            browser.test.sendMessage(`${msg}-done`);
            break;
          default:
            browser.test.fail(`received unexpected message: ${msg}`);
        }
      });

      browser.test.sendMessage("background-ready");
    },
    files: {
      "check-applied-styles.js": () => {
        browser.test.sendMessage(
          `background-color-${location.pathname.split("/").pop()}`,
          getComputedStyle(document.querySelector("#test")).backgroundColor
        );
      },
      "style-1.css": "#test { background-color: rgb(255, 0, 0); }",
      "style-2.css": "#test { background-color: rgb(0, 0, 255); }",
      "style-3.css": "html  { background-color: rgb(0, 255, 0); }",
      "script-document-start.js": async () => {
        const testElement = document.querySelector("html");

        browser.test.assertEq(
          "rgb(0, 255, 0)",
          getComputedStyle(testElement).backgroundColor,
          "got expected style in script-document-start.js"
        );

        testElement.style.backgroundColor = "rgb(4, 4, 4)";
      },
      "check-applied-styles-document-start.js": () => {
        browser.test.sendMessage(
          `background-color-${location.pathname.split("/").pop()}`,
          getComputedStyle(document.querySelector("html")).backgroundColor
        );
      },
      "script-document-end-and-idle.js": () => {
        const testElement = document.querySelector("#test");

        browser.test.assertEq(
          "rgb(255, 0, 0)",
          getComputedStyle(testElement).backgroundColor,
          "got expected style in script-document-end-and-idle.js"
        );

        testElement.style.backgroundColor = "rgb(5, 5, 5)";
      },
    },
  });

  const TEST_CASES = [
    {
      title: "a css file",
      params: [
        {
          id: "style-1",
          allFrames: false,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          // TODO: Bug 1759117 - runAt should not affect css injection
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
        },
      ],
      expected: ["rgb(255, 0, 0)", "rgba(0, 0, 0, 0)"],
    },
    {
      title: "css and allFrames: true",
      params: [
        {
          id: "style-1",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          // TODO: Bug 1759117 - runAt should not affect css injection
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
        },
      ],
      expected: ["rgb(255, 0, 0)", "rgb(255, 0, 0)"],
    },
    {
      title: "css and allFrames: true but matches restricted to top frame",
      params: [
        {
          id: "style-1",
          allFrames: true,
          matches: ["http://*/*/file_with_iframe.html"],
          matchOriginAsFallback: false,
          // TODO: Bug 1759117 - runAt should not affect css injection
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
        },
      ],
      expected: ["rgb(255, 0, 0)", "rgba(0, 0, 0, 0)"],
    },
    {
      title: "css and excludeMatches set",
      params: [
        {
          id: "style-1",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          // TODO: Bug 1759117 - runAt should not affect css injection
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
          excludeMatches: ["http://*/*/file_with_iframe.html"],
        },
      ],
      expected: ["rgba(0, 0, 0, 0)", "rgb(255, 0, 0)"],
    },
    {
      title: "two css files",
      params: [
        {
          id: "style-1-and-2",
          allFrames: false,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          // TODO: Bug 1759117 - runAt should not affect css injection
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css", "style-2.css"],
        },
      ],
      expected: ["rgb(0, 0, 255)", "rgba(0, 0, 0, 0)"],
    },
    {
      title: "two scripts with css",
      params: [
        {
          id: "style-1",
          allFrames: false,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          runAt: "document_end",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
        },
        {
          id: "style-2",
          allFrames: false,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-2.css"],
        },
      ],
      // TODO: Bug 1759117 - The expected value should be `rgb(0, 0, 255)`
      // because runAt should not affect css injection and therefore the two
      // styles should be applied one after the other.
      expected: ["rgb(255, 0, 0)", "rgba(0, 0, 0, 0)"],
    },
    {
      title: "js and css with runAt: document_start",
      params: [
        {
          id: "js-and-style-start",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          runAt: "document_start",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-3.css"],
          // Inject the check script last to be able to send a message back to
          // the test case. This works with `skipCheckScriptRegistration: true`
          // below.
          js: [
            "script-document-start.js",
            "check-applied-styles-document-start.js",
          ],
        },
      ],
      expected: ["rgb(4, 4, 4)", "rgb(4, 4, 4)"],
      skipCheckScriptRegistration: true,
    },
    {
      title: "js and css with runAt: document_end",
      params: [
        {
          id: "js-and-style-end",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          runAt: "document_end",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
          // Inject the check script last to be able to send a message back to
          // the test case. This works with `skipCheckScriptRegistration: true`
          // below.
          js: ["script-document-end-and-idle.js", "check-applied-styles.js"],
        },
      ],
      expected: ["rgb(5, 5, 5)", "rgb(5, 5, 5)"],
      skipCheckScriptRegistration: true,
    },
    {
      title: "js and css with runAt: document_idle",
      params: [
        {
          id: "js-and-style-idle",
          allFrames: true,
          matches: ["http://*/*/*.html"],
          matchOriginAsFallback: false,
          runAt: "document_idle",
          world: "ISOLATED",
          persistAcrossSessions: false,
          css: ["style-1.css"],
          // Inject the check script last to be able to send a message back to
          // the test case. This works with `skipCheckScriptRegistration: true`
          // below.
          js: ["script-document-end-and-idle.js", "check-applied-styles.js"],
        },
      ],
      expected: ["rgb(5, 5, 5)", "rgb(5, 5, 5)"],
      skipCheckScriptRegistration: true,
    },
  ];

  await extension.startup();
  await extension.awaitMessage("background-ready");

  for (const {
    title,
    params,
    expected,
    skipCheckScriptRegistration,
  } of TEST_CASES) {
    extension.sendMessage("load-test-case", {
      title,
      params,
      skipCheckScriptRegistration,
    });
    await extension.awaitMessage("load-test-case-done");

    let contentPage = await ExtensionTestUtils.loadContentPage(
      `${BASE_URL}/file_with_iframe.html`
    );

    const backgroundColors = [
      await extension.awaitMessage("background-color-file_with_iframe.html"),
      await extension.awaitMessage("background-color-file_sample.html"),
    ];

    Assert.deepEqual(
      expected,
      backgroundColors,
      `${title} - got expected colors`
    );

    await contentPage.close();
  }

  await extension.unload();
});
