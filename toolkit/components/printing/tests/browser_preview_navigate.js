/* any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function test_preview_navigate() {
  // Making the test work with the old UI is harder than needed, and the old UI
  // is going away soon anyways.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await BrowserTestUtils.withNewTab(
    `data:text/html,<a style="display: block; width: 100%; height: 100%;" href="https://example.com">Navigate away</a>`,
    async function(browser) {
      document.getElementById("cmd_print").doCommand();
      info("waiting for print");
      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

      let previewBrowser = document.querySelector(".printPreviewBrowser");

      // Wait for the preview operation to actually finish.
      info("waiting for preview to finish");
      await BrowserTestUtils.waitForCondition(() => {
        return SpecialPowers.spawn(previewBrowser, [], () => {
          return !!content.document.querySelector("a");
        });
      });

      // This should do nothing, it won't navigate the preview tab away.
      info("waiting for useless click");
      await BrowserTestUtils.synthesizeMouse("a", 1, 1, {}, previewBrowser);

      let newTabOpen = BrowserTestUtils.waitForNewTab(gBrowser, null);

      info("waiting for useful click");
      await BrowserTestUtils.synthesizeMouse(
        "a",
        1,
        1,
        { ctrlKey: true },
        previewBrowser
      );
      info("waiting for tab to open");
      let newTab = await newTabOpen;
      info("removing new tab");
      await BrowserTestUtils.removeTab(newTab);
      info("ensuring content is still there");
      let contentFound = await SpecialPowers.spawn(previewBrowser, [], () => {
        return !!content.document.querySelector("a");
      });

      ok(
        contentFound,
        "Should not be able to navigate the preview away, " +
          "but should be able to open it in a new window"
      );
    }
  );
});

add_task(async function test_preview_navigate_oop() {
  // Making the test work with the old UI is double the work, and the old UI is
  // going away soon anyways.
  await SpecialPowers.pushPrefEnv({
    set: [["print.tab_modal.enabled", true]],
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  // NOTE(emilio): Intentional http / https difference here to test the OOP
  // iframe case with Fission.
  let frameUrl = new URL("https://example.com/document-builder.sjs");
  frameUrl.searchParams.append(
    "html",
    `
      <!doctype html>
      Whatever
      <script>
        onload = function() {
          setTimeout(() => print(), 0);
        };
      </script>
    `
  );

  let outerUrl = new URL("http://example.com/document-builder.sjs");
  outerUrl.searchParams.append(
    "html",
    `
      <!doctype html>
      <iframe src="${frameUrl.href}"></iframe>
      <a href="/away">Navigate away</a>
      <script>
        onload = function() {
          let div = document.createElement("div");
          div.id = "load-fired";
          document.body.appendChild(div);
          setTimeout(function() {
            document.querySelector("a").click();
          }, 0);
        };
      </script>
    `
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: outerUrl.href,
      waitForLoad: false,
    },
    async function(browser) {
      info("waiting for print");
      await BrowserTestUtils.waitForCondition(
        () => !!document.querySelector(".printPreviewBrowser")
      );

      let loadFired = await SpecialPowers.spawn(browser, [], () => {
        return !!content.document.querySelector("#load-fired");
      });
      ok(loadFired, "Should've tried to navigate away");

      let previewBrowser = document.querySelector(".printPreviewBrowser");
      isnot(
        previewBrowser,
        null,
        "Shouldn't have navigated away / closed the preview"
      );

      let didNotNavigate = await SpecialPowers.spawn(browser, [], () => {
        return !!content.document.querySelector("#load-fired");
      });
      ok(didNotNavigate, "Shouldn't have navigated");
    }
  );
});
