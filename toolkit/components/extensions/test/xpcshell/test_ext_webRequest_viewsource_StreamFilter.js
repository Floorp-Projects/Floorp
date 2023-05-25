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
          browser.test.notifyPass("filter_end");
        };
        filter.onerror = () => {
          browser.test.fail(`Unexpected error: ${filter.error}`);
          browser.test.notifyFail("filter_end");
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
        filter.onerror = () => {
          browser.test.assertEq(
            // TODO bug 1683862: must be "Channel redirected", but it is not
            // because document requests are handled differently compared to
            // other requests, see the comment at the top of
            // test_ext_webRequest_redirect_StreamFilter.js.
            "Invalid request ID",
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
  await extension.awaitFinish("filter_end");
  let contentText = await contentPage.spawn([], () => {
    return this.content.document.body.textContent;
  });
  equal(contentText, "PREFIX_ok_SUFFIX", "view-source response body");
  await contentPage.close();
  await extension.unload();
}

add_task(async function test_StreamFilter_viewsource() {
  await testViewSource(`view-source:${BASE_URL}/dummy`);
});

add_task(async function test_StreamFilter_viewsource_redirect_target() {
  await testViewSource(`view-source:${BASE_URL}/redir`);
});

// Sanity check: nothing bad happens if the underlying response is aborted.
add_task(async function test_StreamFilter_viewsource_cancel() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "*://*/*"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          let filter = browser.webRequest.filterResponseData(details.requestId);
          filter.onstart = () => {
            filter.disconnect();
            browser.test.fail("Unexpected filter.onstart");
            browser.test.notifyFail("filter_end");
          };
          filter.onerror = () => {
            browser.test.assertEq("Invalid request ID", filter.error, "Error?");
            browser.test.notifyPass("filter_end");
          };
        },
        { urls: ["*://*/dummy"] },
        ["blocking"]
      );
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          browser.test.log("Intentionally canceling view-source request");
          return { cancel: true };
        },
        { urls: ["*://*/dummy"] },
        ["blocking"]
      );
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/dummy`
  );
  await extension.awaitFinish("filter_end");
  let contentText = await contentPage.spawn([], () => {
    return this.content.document.body.textContent;
  });
  equal(contentText, "", "view-source request should have been canceled");
  await contentPage.close();
  await extension.unload();
});
