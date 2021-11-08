/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BUILDER_URL = "https://example.com/document-builder.sjs?html=";
const PAGE_MARKUP = `
<html>
  <head>
    <style>
      body {
        background-color: rgb(0, 255, 0);
        margin: 0;
      }

      div {
        background-color: rgb(255, 0, 0);
        height: 100vh;
        width: 100vw;
        margin-top: 100vh;
      }
    </style>
  </head>
  <body>
    <div id="bigredblock"></div>
  </body>
</html>
`;
const PAGE_URL = BUILDER_URL + encodeURI(PAGE_MARKUP);

/**
 * These tests ensure that it's possible to capture the full viewport of
 * a browser, and not just the top region.
 */
add_task(async function thumbnails_fullViewport_capture() {
  // Create a tab with a green background.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE_URL,
    },
    async browser => {
      let canvas = PageThumbs.createCanvas(window);
      await PageThumbs.captureToCanvas(browser, canvas, {
        fullViewport: true,
      });

      // The red region isn't scrolled in yet, so we should get
      // a green thumbnail.
      let ctx = canvas.getContext("2d");
      let [r, g, b] = ctx.getImageData(0, 0, 1, 1).data;
      Assert.equal(r, 0, "No red channel");
      Assert.equal(g, 255, "Full green channel");
      Assert.equal(b, 0, "No blue channel");

      // Now scroll the red region into view.
      await SpecialPowers.spawn(browser, [], () => {
        let redblock = content.document.getElementById("bigredblock");
        redblock.scrollIntoView(true);
      });

      await PageThumbs.captureToCanvas(browser, canvas, {
        fullViewport: true,
      });

      // The captured region should be red.
      ctx = canvas.getContext("2d");
      [r, g, b] = ctx.getImageData(0, 0, 1, 1).data;
      Assert.equal(r, 255, "Full red channel");
      Assert.equal(g, 0, "No green channel");
      Assert.equal(b, 0, "No blue channel");
    }
  );
});
