/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);

var content = "line 1\nline 2\nline 3";

add_task(async function() {
  // First test with text with the text/html mimetype.
  let tab = await openDocument("data:text/html," + encodeURIComponent(content));
  await checkViewSource(tab);
  gBrowser.removeTab(tab);

  tab = await openDocument("data:text/plain," + encodeURIComponent(content));
  await checkViewSource(tab);
  gBrowser.removeTab(tab);
});

var checkViewSource = async function(aTab) {
  let browser = aTab.linkedBrowser;
  await SpecialPowers.spawn(browser, [content], async function(text) {
    is(content.document.body.textContent, text, "Correct content loaded");
  });

  for (let i = 1; i <= 3; i++) {
    browser.sendMessageToActor(
      "ViewSource:GoToLine",
      {
        lineNumber: i,
      },
      "ViewSourcePage"
    );
    await SpecialPowers.spawn(browser, [i], async function(i) {
      let selection = content.getSelection();
      Assert.equal(selection.toString(), "line " + i, "Correct text selected");
    });
  }
};
