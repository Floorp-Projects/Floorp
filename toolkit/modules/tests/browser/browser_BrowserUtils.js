/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/BrowserUtils.jsm");

add_task(async function test_getSelectionDetails_input() {
  // Mostly a regression test for bug 1420560
  const url = kFixtureBaseURL + "file_getSelectionDetails_inputs.html";
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async browser => {
    await ContentTask.spawn(browser, null, () => {
      function checkSelection({ id, text, linkURL }) {
        content.document.getElementById(id).select();
        // It seems that when running as a test, the previous line will set
        // the both the window's selection and the input's selection to contain
        // the input's text. Outside of tests, only the input's selection seems
        // to be updated, so we explicitly clear the window's selection to
        // ensure we're doing the right thing in the case that only the input's
        // selection is present.
        content.getSelection().removeAllRanges();
        let info = BrowserUtils.getSelectionDetails(content);
        Assert.equal(text, info.text);
        Assert.ok(!info.collapsed);
        Assert.equal(linkURL, info.linkURL);
      }

      checkSelection({
        id: "url-no-scheme",
        text: "test.example.com",
        linkURL: "http://test.example.com/"
      });
      checkSelection({
        id: "url-with-scheme",
        text: "https://test.example.com",
        linkURL: "https://test.example.com/"
      });
      checkSelection({
        id: "not-url",
        text: "foo. bar",
        linkURL: null
      });
      checkSelection({
        id: "not-url-number",
        text: "3.5",
        linkURL: null
      });
    });
  });
});
