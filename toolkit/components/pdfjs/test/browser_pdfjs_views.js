/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      // check that PDF is opened with internal viewer
      await waitForPdfJS(browser, TESTROOT + "file_pdfjs_test.pdf");

      await SpecialPowers.spawn(browser, [], async function() {
        Assert.ok(
          content.document.querySelector("div#viewer"),
          "document content has viewer UI"
        );

        // open sidebar
        var sidebar = content.document.querySelector("button#sidebarToggle");
        var outerContainer = content.document.querySelector(
          "div#outerContainer"
        );

        sidebar.click();
        Assert.ok(
          outerContainer.classList.contains("sidebarOpen"),
          "sidebar opens on click"
        );

        // check that thumbnail view is open
        var thumbnailView = content.document.querySelector("div#thumbnailView");
        var outlineView = content.document.querySelector("div#outlineView");

        Assert.equal(
          thumbnailView.getAttribute("class"),
          null,
          "Initial view is thumbnail view"
        );
        Assert.equal(
          outlineView.getAttribute("class"),
          "hidden",
          "Outline view is hidden initially"
        );

        // switch to outline view
        var viewOutlineButton = content.document.querySelector(
          "button#viewOutline"
        );
        viewOutlineButton.click();

        Assert.equal(
          thumbnailView.getAttribute("class"),
          "hidden",
          "Thumbnail view is hidden when outline is selected"
        );
        Assert.equal(
          outlineView.getAttribute("class"),
          "",
          "Outline view is visible when selected"
        );

        // switch back to thumbnail view
        var viewThumbnailButton = content.document.querySelector(
          "button#viewThumbnail"
        );
        viewThumbnailButton.click();

        Assert.equal(
          thumbnailView.getAttribute("class"),
          "",
          "Thumbnail view is visible when selected"
        );
        Assert.equal(
          outlineView.getAttribute("class"),
          "hidden",
          "Outline view is hidden when thumbnail is selected"
        );

        sidebar.click();

        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
