"use strict";

// Test that an extension page which is sandboxed may load resources
// from itself without relying on web acessible resources.
add_task(async function test_webext_background_sandbox_privileges() {
  function backgroundSubframeScript() {
    window.parent.postMessage(typeof browser, "*");
  }

  function backgroundScript() {
    /* eslint-disable-next-line mozilla/balanced-listeners */
    window.addEventListener("message", event => {
      if (event.data == "undefined") {
        browser.test.notifyPass("webext-background-sandbox-privileges");
      } else {
        browser.test.notifyFail("webext-background-sandbox-privileges");
      }
    });
  }

  let extensionData = {
    manifest: {
      background: {
        page: "background.html",
      },
    },
    files: {
      "background.html": `<!DOCTYPE>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="background.js"><\/script>
            <iframe src="background-subframe.html" sandbox="allow-scripts"></iframe>
          </body>
        </html>`,
      "background-subframe.html": `<!DOCTYPE>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="background-subframe.js"><\/script>
          </head>
        </html>`,
      "background-subframe.js": backgroundSubframeScript,
      "background.js": backgroundScript,
    },
  };
  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();

  await extension.awaitFinish("webext-background-sandbox-privileges");
  await extension.unload();
});
