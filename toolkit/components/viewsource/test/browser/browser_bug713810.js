/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const source = '<html xmlns="http://www.w3.org/1999/xhtml"><body><p>This is a paragraph.</p></body></html>';

add_task(function *() {
  let viewSourceTab = yield* openDocumentSelect("data:text/html," + source, "p");
  let text = yield ContentTask.spawn(viewSourceTab.linkedBrowser, { }, function* () {
    return content.document.body.textContent;
  });
  is(text, "<p>This is a paragraph.</p>", "Correct source for text/html");
  gBrowser.removeTab(viewSourceTab);

  viewSourceTab = yield* openDocumentSelect("data:application/xhtml+xml," + source, "p");
  text = yield ContentTask.spawn(viewSourceTab.linkedBrowser, { }, function* () {
    return content.document.body.textContent;
  });
  is(text, '<p xmlns="http://www.w3.org/1999/xhtml">This is a paragraph.</p>',
     "Correct source for application/xhtml+xml");
  gBrowser.removeTab(viewSourceTab);
});

