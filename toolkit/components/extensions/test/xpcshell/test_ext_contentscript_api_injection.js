"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_task(async function test_contentscript_api_injection() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
      web_accessible_resources: ["content_script_iframe.html"],
    },

    files: {
      "content_script.js"() {
        let iframe = document.createElement("iframe");
        iframe.src = browser.runtime.getURL("content_script_iframe.html");
        document.body.appendChild(iframe);
      },
      "content_script_iframe.js"() {
        window.location = `http://example.com/data/file_privilege_escalation.html`;
      },
      "content_script_iframe.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script type="text/javascript" src="content_script_iframe.js"></script>
          </head>
        </html>`,
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  let awaitConsole = new Promise(resolve => {
    Services.console.registerListener(function listener(message) {
      if (/WebExt Privilege Escalation/.test(message.message)) {
        Services.console.unregisterListener(listener);
        resolve(message);
      }
    });
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  let message = await awaitConsole;
  ok(
    message.message.includes(
      "WebExt Privilege Escalation: typeof(browser) = undefined"
    ),
    "Document does not have `browser` APIs."
  );

  await contentPage.close();

  await extension.unload();
});
