/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

var content = "line 1\nline 2\nline 3";

add_task(async function() {
  // First test with text with the text/html mimetype.
  let win = await loadViewSourceWindow("data:text/html," + encodeURIComponent(content));
  await checkViewSource(win);
  await BrowserTestUtils.closeWindow(win);

  win = await loadViewSourceWindow("data:text/plain," + encodeURIComponent(content));
  await checkViewSource(win);
  await BrowserTestUtils.closeWindow(win);
});

var checkViewSource = async function(aWindow) {
  is(aWindow.gBrowser.contentDocument.body.textContent, content, "Correct content loaded");
  let statusPanel = aWindow.document.getElementById("statusbar-line-col");
  is(statusPanel.getAttribute("label"), "", "Correct status bar text");

  for (let i = 1; i <= 3; i++) {
    aWindow.viewSourceChrome.goToLine(i);
    await ContentTask.spawn(aWindow.gBrowser, i, async function(i) {
      let selection = content.getSelection();
      Assert.equal(selection.toString(), "line " + i, "Correct text selected");
    });

    await ContentTaskUtils.waitForCondition(() => {
      return (statusPanel.getAttribute("label") == "Line " + i + ", Col 1");
    }, "Correct status bar text");
  }
};
