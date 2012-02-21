/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var source = '<html xmlns="http://www.w3.org/1999/xhtml"><body><p>This is a paragraph.</p></body></html>';

function test() {
  waitForExplicitFinish();
  testHTML();
}

function testHTML() {
  openDocumentSelect("data:text/html," + source, "p", function(aWindow) {
    is(aWindow.gBrowser.contentDocument.body.textContent,
       "<p>This is a paragraph.</p>",
       "Correct source for text/html");
    closeViewSourceWindow(aWindow, testXHTML);
  });
}

function testXHTML() {
  openDocumentSelect("data:application/xhtml+xml," + source, "p", function(aWindow) {
    is(aWindow.gBrowser.contentDocument.body.textContent,
       '<p xmlns="http://www.w3.org/1999/xhtml">This is a paragraph.</p>',
       "Correct source for application/xhtml+xml");
    closeViewSourceWindow(aWindow, finish);
  });
}
