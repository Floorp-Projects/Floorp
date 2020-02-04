/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("layout.xml.prettyprint", true);

const BASE_XML = '<?xml version="1.0" encoding="UTF-8"?>';
const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/test.xml", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/xml; charset=utf-8", false);
  response.write(`${BASE_XML}\n<note></note>`);
});

// Make sure that XML pretty printer runs after content scripts
// that runs at document_start (See Bug 1605657).
add_task(async function content_script_on_xml_prettyprinted_document() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["<all_urls>"],
          js: ["start.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "start.js": async function() {
        const el = document.createElement("ext-el");
        document.documentElement.append(el);
        if (document.readyState !== "complete") {
          await new Promise(resolve => {
            document.addEventListener("DOMContentLoaded", resolve, {
              once: true,
            });
          });
        }
        browser.test.sendMessage("content-script-done");
      },
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/test.xml"
  );

  info("Wait content script and xml document to be fully loaded");
  await extension.awaitMessage("content-script-done");

  info("Verify the xml file is still pretty printed");
  const res = await contentPage.spawn([], () => {
    const doc = this.content.document;
    const shadowRoot = doc.documentElement.openOrClosedShadowRoot;
    const prettyPrintLink =
      shadowRoot &&
      shadowRoot.querySelector("link[href*='XMLPrettyPrint.css']");
    return {
      hasShadowRoot: !!shadowRoot,
      hasPrettyPrintLink: !!prettyPrintLink,
    };
  });

  Assert.deepEqual(
    res,
    { hasShadowRoot: true, hasPrettyPrintLink: true },
    "The XML file has the pretty print shadowRoot"
  );

  await contentPage.close();
  await extension.unload();
});
