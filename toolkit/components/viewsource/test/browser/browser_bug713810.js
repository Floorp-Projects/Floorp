/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const source =
  '<html xmlns="http://www.w3.org/1999/xhtml"><body><p>This is a paragraph.</p></body></html>';

add_task(async function() {
  let viewSourceTab = await openDocumentSelect("data:text/html," + source, "p");
  await SpecialPowers.spawn(viewSourceTab.linkedBrowser, [], async function() {
    Assert.equal(
      content.document.body.textContent,
      "<p>This is a paragraph.</p>",
      "Correct source for text/html"
    );
  });
  gBrowser.removeTab(viewSourceTab);

  viewSourceTab = await openDocumentSelect(
    "data:application/xhtml+xml," + source,
    "p"
  );
  await SpecialPowers.spawn(viewSourceTab.linkedBrowser, [], async function() {
    Assert.equal(
      content.document.body.textContent,
      '<p xmlns="http://www.w3.org/1999/xhtml">This is a paragraph.</p>',
      "Correct source for application/xhtml+xml"
    );
  });
  gBrowser.removeTab(viewSourceTab);
});
