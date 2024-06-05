/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_URI =
  "http://mochi.test:8888/browser/netwerk/test/browser/res_hello_h1.sjs?type=";

async function expectOutcome(aContentType, aBehavior) {
  info(`Expecting ${aContentType} to be loaded as ${aBehavior}`);
  const url = BASE_URI + aContentType;

  await BrowserTestUtils.withNewTab(url, async browser => {
    await SpecialPowers.spawn(browser, [url, aBehavior], (url, aBehavior) => {
      is(content.location.href, url, "expected url was loaded");
      switch (aBehavior) {
        case "html":
          is(
            content.document.querySelector("h1").textContent,
            "hello",
            "parsed as HTML, so document should contain an <h1> element"
          );
          break;
        case "text":
          is(
            content.document.body.textContent,
            "<h1>hello</h1>",
            "parsed as text, so document should contain bare text"
          );
          break;
        case "jsonviewer":
          ok(
            content.wrappedJSObject.JSONView,
            "page has loaded the DevTools JSONViewer"
          );
          break;
        default:
          ok(false, "unexpected behavior");
          break;
      }
    });
  });
}

add_task(async function test_display_plaintext_type() {
  // Make sure that if the data is HTML it loads as HTML.
  await expectOutcome("text/html", "html");

  // For other text-like types, make sure we load as plain text.
  await expectOutcome("text/plain", "text");

  await expectOutcome("application/ecmascript", "text");
  await expectOutcome("application/javascript", "text");
  await expectOutcome("application/x-javascript", "text");
  await expectOutcome("text/cache-manifest", "text");
  await expectOutcome("text/css", "text");
  await expectOutcome("text/ecmascript", "text");
  await expectOutcome("text/event-stream", "text");
  await expectOutcome("text/javascript", "text");

  await expectOutcome("application/json", "jsonviewer");
  // NOTE: text/json does not load JSON viewer?
  await expectOutcome("text/json", "text");

  // Unknown text/ types should be loadable as plain text documents.
  await expectOutcome("text/unknown-type", "text");
});
