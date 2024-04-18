/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_getSelectionDetails_input() {
  // Mostly a regression test for bug 1420560
  const url = kFixtureBaseURL + "file_getSelectionDetails_inputs.html";
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async browser => {
    await SpecialPowers.spawn(browser, [], () => {
      function checkSelection({ id, text, linkURL }) {
        const { SelectionUtils } = ChromeUtils.importESModule(
          "resource://gre/modules/SelectionUtils.sys.mjs"
        );
        content.document.getElementById(id).select();
        // It seems that when running as a test, the previous line will set
        // the both the window's selection and the input's selection to contain
        // the input's text. Outside of tests, only the input's selection seems
        // to be updated, so we explicitly clear the window's selection to
        // ensure we're doing the right thing in the case that only the input's
        // selection is present.
        content.getSelection().removeAllRanges();
        let info = SelectionUtils.getSelectionDetails(content);
        Assert.equal(text, info.text);
        Assert.strictEqual(info.docSelectionIsCollapsed, false);
        Assert.equal(linkURL, info.linkURL);
      }

      checkSelection({
        id: "url-no-scheme",
        text: "test.example.com",
        linkURL: "http://test.example.com/",
      });
      checkSelection({
        id: "url-with-scheme",
        text: "https://test.example.com",
        linkURL: "https://test.example.com/",
      });
      checkSelection({
        id: "not-url",
        text: "foo. bar",
        linkURL: null,
      });
      checkSelection({
        id: "not-url-number",
        text: "3.5",
        linkURL: null,
      });
    });
  });
});

add_task(async function test_getSelectionDetails_shadow_selection() {
  const url = kFixtureBaseURL + "file_getSelectionDetails_inputs.html";
  await SpecialPowers.pushPrefEnv({
    set: [["dom.shadowdom.selection_across_boundary.enabled", true]],
  });
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      function checkSelection() {
        const { SelectionUtils } = ChromeUtils.importESModule(
          "resource://gre/modules/SelectionUtils.sys.mjs"
        );

        const text = content.document.getElementById("outer");
        const host = content.document.getElementById("host");
        content
          .getSelection()
          .setBaseAndExtent(
            text,
            0,
            host.shadowRoot.getElementById("inner").firstChild,
            3
          );
        let info = SelectionUtils.getSelectionDetails(content);
        // TODO(sefeng): verify info.text after bug 1881095 is fixed
        Assert.strictEqual(info.docSelectionIsCollapsed, false);
      }
      checkSelection();
    });
  });
});
