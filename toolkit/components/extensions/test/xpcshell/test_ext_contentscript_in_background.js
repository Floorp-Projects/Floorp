/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/dummyFrame", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("");
});

add_task(async function content_script_in_background_frame() {
  // Test loads http: frame in background page.
  allow_unsafe_parent_loads_when_extensions_not_remote();

  async function background() {
    const FRAME_URL = "http://example.com:8888/dummyFrame";
    await browser.contentScripts.register({
      matches: ["http://example.com/dummyFrame"],
      js: [{ file: "contentscript.js" }],
      allFrames: true,
    });

    let f = document.createElement("iframe");
    f.src = FRAME_URL;
    document.body.appendChild(f);
  }

  function contentScript() {
    browser.test.log(`Running content script at ${document.URL}`);
    browser.test.sendMessage("done_in_content_script");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://example.com/*"],
    },
    files: {
      "contentscript.js": contentScript,
    },
    background,
  });
  await extension.startup();
  await extension.awaitMessage("done_in_content_script");
  await extension.unload();

  revert_allow_unsafe_parent_loads_when_extensions_not_remote();
});
