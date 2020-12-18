"use strict";

const server = createHttpServer();
const BASE_URL = `http://127.0.0.1:${server.identity.primaryPort}`;

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

server.registerPathHandler("/redir", (request, response) => {
  response.setStatusLine(request.httpVersion, 303, "See Other");
  response.setHeader("Location", `${BASE_URL}/dummy`);
});

async function testViewSource(viewSourceUrl) {
  function background(BASE_URL) {
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.assertEq(`${BASE_URL}/dummy`, details.url, "expected URL");
        browser.test.assertEq("main_frame", details.type, "details.type");

        let filter = browser.webRequest.filterResponseData(details.requestId);
        filter.onstart = () => {
          filter.write(new TextEncoder().encode("PREFIX_"));
        };
        filter.ondata = event => {
          filter.write(event.data);
        };
        filter.onstop = () => {
          filter.write(new TextEncoder().encode("_SUFFIX"));
          filter.disconnect();
          browser.test.sendMessage("filter_end");
        };
        filter.onerror = function() {
          browser.test.fail(`Unexpected error: ${filter.error}`);
          browser.test.sendMessage("filter_end");
        };
      },
      { urls: ["*://*/dummy"] },
      ["blocking"]
    );
    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.assertEq(`${BASE_URL}/redir`, details.url, "Got redirect");

        let filter = browser.webRequest.filterResponseData(details.requestId);
        filter.onstop = () => {
          filter.disconnect();
          browser.test.fail("Unexpected onstop for redirect");
          browser.test.sendMessage("redirect_done");
        };
        filter.onerror = function() {
          browser.test.assertEq(
            "Channel redirected",
            filter.error,
            "Expected error in filter.onerror"
          );
          browser.test.sendMessage("redirect_done");
        };
      },
      { urls: ["*://*/redir"] },
      ["blocking"]
    );
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "*://*/*"],
    },
    background: `(${background})(${JSON.stringify(BASE_URL)})`,
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(viewSourceUrl);
  if (viewSourceUrl.includes("/redir")) {
    info("Awaiting observed completion of redirection request");
    await extension.awaitMessage("redirect_done");
  }
  info("Awaiting completion of StreamFilter on request");
  await extension.awaitMessage("filter_end");
  let contentText = await contentPage.spawn(null, () => {
    return this.content.document.body.textContent;
  });
  equal(contentText, "PREFIX_ok_SUFFIX", "view-source response body");
  await contentPage.close();
  await extension.unload();
}

add_task(async function test_StreamFilter_viewsource() {
  await testViewSource(`view-source:${BASE_URL}/dummy`);
});

// Skipped because filter.error is incorrect and test non-deterministic, see
// https://bugzilla.mozilla.org/show_bug.cgi?id=1683189
// add_task(async function test_StreamFilter_viewsource_redirect_target() {
//   await testViewSource(`view-source:${BASE_URL}/redir`);
// });
