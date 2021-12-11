/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getDataFromNextSubmitMessage() {
  return new Promise(resolve => {
    LoginManagerParent.setListenerForTests((msg, data) => {
      if (msg == "FormSubmit") {
        resolve(data);
      }
    });
  });
}

add_task(async function testCrossOriginFormUsesCorrectOrigin() {
  const registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
    Ci.nsIChromeRegistry
  );

  let dataPromise = getDataFromNextSubmitMessage();

  let url =
    registry.convertChromeURL(Services.io.newURI(getRootDirectory(gTestPath)))
      .asciiSpec + "form_basic.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      await SpecialPowers.spawn(browser.browsingContext, [], () => {
        let doc = content.document;
        doc.getElementById("form-basic-username").setUserInput("username");
        doc.getElementById("form-basic-password").setUserInput("password");
        doc.getElementById("form-basic").submit();
        info("Submitting form");
      });
    }
  );

  let data = await dataPromise;
  info("Origin retrieved from message listener");

  is(data.origin, "file://", "Message origin should match form origin");
});
