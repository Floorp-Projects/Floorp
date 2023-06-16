"use strict";

// StreamFilters should be closed upon a redirect.
//
// Some redirects are already tested in other tests:
// - test_ext_webRequest_filterResponseData.js tests fetch requests.
// - test_ext_webRequest_viewsource_StreamFilter.js tests view-source documents.
//
// Usually, redirects are caught in StreamFilterParent::OnStartRequest, but due
// to the fact that AttachStreamFilter is deferred for document requests, OSR is
// not called and the cleanup is triggered from nsHttpChannel::ReleaseListeners.

const server = createHttpServer({ hosts: ["example.com", "example.org"] });

server.registerPathHandler("/redir", (request, response) => {
  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", "/target");
});
server.registerPathHandler("/target", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});
server.registerPathHandler("/RedirectToRedir.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8");
  response.write("<script>location.href='http://example.com/redir';</script>");
});
server.registerPathHandler("/iframeWithRedir.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8");
  response.write("<iframe src='http://example.com/redir'></iframe>");
});

function loadRedirectCatcherExtension() {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "*://*/*"],
    },
    background() {
      const closeCounts = {};
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          let expectedError = "Channel redirected";
          if (details.type === "main_frame" || details.type === "sub_frame") {
            // Message differs for the reason stated at the top of this file.
            // TODO bug 1683862: Make error message more accurate.
            expectedError = "Invalid request ID";
          }

          closeCounts[details.requestId] = 0;

          let filter = browser.webRequest.filterResponseData(details.requestId);
          filter.onstart = () => {
            filter.disconnect();
            browser.test.fail("Unexpected filter.onstart");
          };
          filter.onerror = function () {
            closeCounts[details.requestId]++;
            browser.test.assertEq(expectedError, filter.error, "filter.error");
          };
        },
        { urls: ["*://*/redir"] },
        ["blocking"]
      );
      browser.webRequest.onCompleted.addListener(
        details => {
          // filter.onerror from the redirect request should be called before
          // webRequest.onCompleted of the redirection target. Regression test
          // for bug 1683189.
          browser.test.assertEq(
            1,
            closeCounts[details.requestId],
            "filter from initial, redirected request should have been closed"
          );
          browser.test.log("Intentionally canceling view-source request");
          browser.test.sendMessage("req_end", details.type);
        },
        { urls: ["*://*/target"] }
      );
    },
  });
}

add_task(async function redirect_document() {
  let extension = loadRedirectCatcherExtension();
  await extension.startup();

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/redir"
    );
    equal(await extension.awaitMessage("req_end"), "main_frame", "is top doc");
    await contentPage.close();
  }

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.com/iframeWithRedir.html"
    );
    equal(await extension.awaitMessage("req_end"), "sub_frame", "is sub doc");
    await contentPage.close();
  }

  await extension.unload();
});

// Cross-origin redirect = process switch.
add_task(async function redirect_document_cross_origin() {
  let extension = loadRedirectCatcherExtension();
  await extension.startup();

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.org/RedirectToRedir.html"
    );
    equal(await extension.awaitMessage("req_end"), "main_frame", "is top doc");
    await contentPage.close();
  }

  {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      "http://example.org/iframeWithRedir.html"
    );
    equal(await extension.awaitMessage("req_end"), "sub_frame", "is sub doc");
    await contentPage.close();
  }

  await extension.unload();
});
