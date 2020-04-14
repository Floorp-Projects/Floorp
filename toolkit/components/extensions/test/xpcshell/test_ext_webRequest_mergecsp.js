/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
    browser.test.onMessage.addListener(function(msg, expectedCount) {
      csp_value = msg;
    });
    browser.webRequest.onHeadersReceived.addListener(
      e => {
        browser.test.log(`onHeadersReceived ${e.requestId} ${e.url}`);
        if (csp_value === undefined) {
          browser.test.assertTrue(false, "extension called before CSP was set");
        }
        if (csp_value === "") {
          e.responseHeaders = e.responseHeaders.filter(
            i => i.name.toLowerCase() != "content-security-policy"
          );
        } else if (csp_value !== null) {
          e.responseHeaders.push({
            name: "Content-Security-Policy",
            value: csp_value,
          });
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
 * @param {string} site_csp The CSP to be sent by the site, or null.
 * @param {string} ext1_csp The CSP to be sent by the first extension, or null.
 * @param {string} ext2_csp The CSP to be sent by the second extension, or null.
 * @param {Object} expect   Object containing information which resources are expected to be loaded.
 * @param {Object} expect.img1_loaded    image from a first party origin.
 * @param {Object} expect.img3_loaded    image from a third party origin.
 * @param {Object} expect.script1_loaded script from a first party origin.
 * @param {Object} expect.script3_loaded script from a third party origin.
 */
async function test_csp(site_csp, ext1_csp, ext2_csp, expect) {
  let extension1 = await ExtensionTestUtils.loadExtension(extensionData);
  let extension2 = await ExtensionTestUtils.loadExtension(extensionData);
  await extension1.startup();
  await extension2.startup();
  await extension1.sendMessage(ext1_csp);
  await extension2.sendMessage(ext2_csp);

  let csp_value = encodeURIComponent(site_csp || "");
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.net/?${csp_value}`
  );
  let results = await contentPage.spawn(null, async () => {
    let img1 = this.content.document.getElementById("img1");
    let img3 = this.content.document.getElementById("img3");
    return {
      img1_loaded: img1.complete && img1.naturalWidth > 0,
      img3_loaded: img3.complete && img3.naturalWidth > 0,
      // Note: "good" and "bad" are just placeholders; they don't mean anything.
      script1_loaded: !!this.content.document.getElementById("good"),
      script3_loaded: !!this.content.document.getElementById("bad"),
    };
  });

  let action = {
    true: "loaded",
    false: "blocked",
  };

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

  await contentPage.close();
  await extension1.unload();
  await extension2.unload();
}

add_task(async function test_webRequest_mergecsp() {
  await test_csp("default-src *", "script-src 'none'", null, {
    img1_loaded: true,
    img3_loaded: true,
    script1_loaded: false,
    script3_loaded: false,
  });
  await test_csp(null, "script-src 'none'", null, {
    img1_loaded: true,
    img3_loaded: true,
    script1_loaded: false,
    script3_loaded: false,
  });
  await test_csp("default-src *", "script-src 'none'", "img-src 'none'", {
    img1_loaded: false,
    img3_loaded: false,
    script1_loaded: false,
    script3_loaded: false,
  });
  await test_csp(null, "script-src 'none'", "img-src 'none'", {
    img1_loaded: false,
    img3_loaded: false,
    script1_loaded: false,
    script3_loaded: false,
  });
  await test_csp(
    "default-src *",
    "img-src example.com",
    "img-src example.org",
    {
      img1_loaded: false,
      img3_loaded: false,
      script1_loaded: true,
      script3_loaded: true,
    }
  );
  // TODO Bug 1623176: this test hangs on .loadContentPage() when using "img-src
  // 'self'" as the page's CSP, which should result in {true, true, true true}!
  await test_csp("img-src self", "", "img-src example.com", {
    img1_loaded: false,
    img3_loaded: true,
    script1_loaded: true,
    script3_loaded: true,
  });
});
