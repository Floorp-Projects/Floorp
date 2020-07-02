/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

const TESTS = [
  {
    action: {
      selector: "button#zoomIn",
      event: "click",
    },
    expectedZoom: 1, // 1 - zoom in
    message: "Zoomed in using the '+' (zoom in) button",
  },

  {
    action: {
      selector: "button#zoomOut",
      event: "click",
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed out using the '-' (zoom out) button",
  },

  {
    action: {
      keyboard: true,
      keyCode: 61,
      event: "+",
    },
    expectedZoom: 1, // 1 - zoom in
    message: "Zoomed in using the CTRL++ keys",
  },

  {
    action: {
      keyboard: true,
      keyCode: 109,
      event: "-",
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed out using the CTRL+- keys",
  },

  {
    action: {
      selector: "select#scaleSelect",
      index: 5,
      event: "change",
    },
    expectedZoom: -1, // -1 - zoom out
    message: "Zoomed using the zoom picker",
  },
];

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
    async function(newTabBrowser) {
      await waitForPdfJS(
        newTabBrowser,
        TESTROOT + "file_pdfjs_test.pdf#zoom=100"
      );

      await SpecialPowers.spawn(newTabBrowser, [TESTS], async function(
        contentTESTS
      ) {
        let document = content.document;

        function waitForRender() {
          return new Promise(resolve => {
            document.addEventListener(
              "pagerendered",
              function onPageRendered(e) {
                if (e.detail.pageNumber !== 1) {
                  return;
                }

                document.removeEventListener(
                  "pagerendered",
                  onPageRendered,
                  true
                );
                resolve();
              },
              true
            );
          });
        }

        // check that PDF is opened with internal viewer
        Assert.ok(
          content.document.querySelector("div#viewer"),
          "document content has viewer UI"
        );

        let initialWidth, previousWidth;
        initialWidth = previousWidth = parseInt(
          content.document.querySelector("div.page[data-page-number='1']").style
            .width
        );

        for (let subTest of contentTESTS) {
          // We zoom using an UI element
          var ev;
          if (subTest.action.selector) {
            // Get the element and trigger the action for changing the zoom
            var el = document.querySelector(subTest.action.selector);
            Assert.ok(
              el,
              "Element '" + subTest.action.selector + "' has been found"
            );

            if (subTest.action.index) {
              el.selectedIndex = subTest.action.index;
            }

            // Dispatch the event for changing the zoom
            ev = new content.Event(subTest.action.event);
          } else {
            // We zoom using keyboard
            // Simulate key press
            ev = new content.KeyboardEvent("keydown", {
              key: subTest.action.event,
              keyCode: subTest.action.keyCode,
              ctrlKey: true,
            });
            el = content;
          }

          el.dispatchEvent(ev);
          await waitForRender();

          var pageZoomScale = content.document.querySelector(
            "select#scaleSelect"
          );

          // The zoom value displayed in the zoom select
          var zoomValue =
            pageZoomScale.options[pageZoomScale.selectedIndex].innerHTML;

          let pageContainer = content.document.querySelector(
            "div.page[data-page-number='1']"
          );
          let actualWidth = parseInt(pageContainer.style.width);

          // the actual zoom of the PDF document
          let computedZoomValue =
            parseInt((actualWidth / initialWidth).toFixed(2) * 100) + "%";
          Assert.equal(
            computedZoomValue,
            zoomValue,
            "Content has correct zoom"
          );

          // Check that document zooms in the expected way (in/out)
          let zoom = (actualWidth - previousWidth) * subTest.expectedZoom;
          Assert.ok(zoom > 0, subTest.message);

          previousWidth = actualWidth;
        }

        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});

// Performs a SpecialPowers.spawn round-trip to ensure that any setup
// that needs to be done in the content process by any pending tasks has
// a chance to complete before continuing.
function waitForRoundTrip(browser) {
  return SpecialPowers.spawn(browser, [], () => {});
}

async function waitForRenderAndGetWidth(newTabBrowser) {
  return SpecialPowers.spawn(newTabBrowser, [], async function() {
    function waitForRender(document) {
      return new Promise(resolve => {
        document.addEventListener(
          "pagerendered",
          function onPageRendered(e) {
            if (e.detail.pageNumber !== 1) {
              return;
            }

            document.removeEventListener("pagerendered", onPageRendered, true);
            resolve();
          },
          true
        );
      });
    }
    // check that PDF is opened with internal viewer
    Assert.ok(
      content.document.querySelector("div#viewer"),
      "document content has viewer UI"
    );

    await waitForRender(content.document);

    return parseInt(
      content.document.querySelector("div.page[data-page-number='1']").style
        .width
    );
  });
}

add_task(async function test_browser_zoom() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(newTabBrowser) {
      await waitForPdfJS(newTabBrowser, TESTROOT + "file_pdfjs_test.pdf");

      const initialWidth = await waitForRenderAndGetWidth(newTabBrowser);

      // Zoom in
      let newWidthPromise = waitForRenderAndGetWidth(newTabBrowser);
      await waitForRoundTrip(newTabBrowser);
      FullZoom.enlarge();
      ok(
        (await newWidthPromise) > initialWidth,
        "Zoom in makes the page bigger."
      );

      // Reset
      newWidthPromise = waitForRenderAndGetWidth(newTabBrowser);
      await waitForRoundTrip(newTabBrowser);
      FullZoom.reset();
      is(await newWidthPromise, initialWidth, "Zoom reset restores page.");

      // Zoom out
      newWidthPromise = waitForRenderAndGetWidth(newTabBrowser);
      await waitForRoundTrip(newTabBrowser);
      FullZoom.reduce();
      ok(
        (await newWidthPromise) < initialWidth,
        "Zoom out makes the page smaller."
      );

      // Clean-up after the PDF viewer.
      await SpecialPowers.spawn(newTabBrowser, [], function() {
        const viewer = content.wrappedJSObject.PDFViewerApplication;
        return viewer.close();
      });
    }
  );
});
