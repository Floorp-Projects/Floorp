/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let content = "line 1\nline 2\nline 3";
let runningPlainText = false;

function test() {
  waitForExplicitFinish();

  testViewSourceWindow("data:text/html," + encodeURIComponent(content), checkViewSource, function() {
    testViewSourceWindow("data:text/plain," + encodeURIComponent(content), checkViewSource, finish);
  });
}

function checkViewSource(aWindow) {
  is(aWindow.gBrowser.contentDocument.body.textContent, content, "Correct content loaded");

  let selection = aWindow.gBrowser.contentWindow.getSelection();
  let statusPanel = aWindow.document.getElementById("statusbar-line-col");
  is(statusPanel.getAttribute("label"), "", "Correct status bar text");
  for (let i = 1; i <= 3; i++) {
    aWindow.goToLine(i);
    is(selection.toString(), "line " + i, "Correct text selected");
    is(statusPanel.getAttribute("label"), "Line " + i + ", Col 1", "Correct status bar text");
  }
}
