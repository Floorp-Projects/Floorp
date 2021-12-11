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
  let dataPromise = getDataFromNextSubmitMessage();

  let url =
    "https://example.com" +
    DIRECTORY_PATH +
    "form_cross_origin_secure_action.html";

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

  is(
    data.origin,
    "https://example.com",
    "Message origin should match form origin"
  );
  isnot(
    data.origin,
    data.data.actionOrigin,
    "If origin and actionOrigin match, this test will false positive"
  );
});
