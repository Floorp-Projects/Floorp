/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  let source = "about:blank";

  waitForExplicitFinish();
  openViewSourceWindow(source, function(aWindow) {
    let gBrowser = aWindow.gBrowser;

    is(gBrowser.contentDocument.title, source, "Correct document title");
    todo(aWindow.document.documentElement.getAttribute("title") == "Source of: " + source, "Correct window title");
    closeViewSourceWindow(aWindow, finish);
  });
}
