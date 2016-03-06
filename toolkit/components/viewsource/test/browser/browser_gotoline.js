/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

var content = "line 1\nline 2\nline 3";

add_task(function*() {
  // First test with text with the text/html mimetype.
  let win = yield loadViewSourceWindow("data:text/html," + encodeURIComponent(content));
  yield checkViewSource(win);
  yield BrowserTestUtils.closeWindow(win);

  win = yield loadViewSourceWindow("data:text/plain," + encodeURIComponent(content));
  yield checkViewSource(win);
  yield BrowserTestUtils.closeWindow(win);
});

var checkViewSource = Task.async(function* (aWindow) {
  is(aWindow.gBrowser.contentDocument.body.textContent, content, "Correct content loaded");
  let selection = aWindow.gBrowser.contentWindow.getSelection();
  let statusPanel = aWindow.document.getElementById("statusbar-line-col");
  is(statusPanel.getAttribute("label"), "", "Correct status bar text");

  for (let i = 1; i <= 3; i++) {
    aWindow.viewSourceChrome.goToLine(i);
    yield ContentTask.spawn(aWindow.gBrowser, i, function*(i) {
      let selection = content.getSelection();
      Assert.equal(selection.toString(), "line " + i, "Correct text selected");
    });

    yield ContentTaskUtils.waitForCondition(() => {
      return (statusPanel.getAttribute("label") == "Line " + i + ", Col 1");
    }, "Correct status bar text");
  }
});
