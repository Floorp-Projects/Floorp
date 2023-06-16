/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Test for bug 1539759
 * Loads a chrome XML document that has an exteernal DTD file with an entity
 * that contains an element, and verifies that that element was not inserted
 * into the document (but its content was).
 */

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    getRootDirectory(gTestPath) + "browser_elementindtd.xml",
    async function (newBrowser) {
      // NB: We load the chrome:// page in the parent process.
      testNoElementFromEntity(newBrowser);
    }
  );
});

function testNoElementFromEntity(newBrowser) {
  let doc = newBrowser.contentDocument;
  is(doc.body.textContent, "From dtd", "Should load DTD.");
  is(
    doc.body.firstElementChild,
    null,
    "Shouldn't have an element inserted from the DTD"
  );
}
