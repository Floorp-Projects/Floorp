/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const frameSource =
  "<a href='about:mozilla'>some text</a><a id='other' href='about:about'>other text</a>";
const sources = [
  `<html><iframe id="f" srcdoc="${frameSource}"></iframe></html>`,
  `<html><iframe id="f" src="https://example.com/document-builder.sjs?html=${frameSource}"></iframe></html>`,
];

add_task(async function partial_source() {
  for (let source of sources) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "data:text/html," + source
    );

    let frameBC = gBrowser.selectedBrowser.browsingContext.children[0];

    await SpecialPowers.spawn(frameBC, [], () => {
      let element = content.document.getElementById("other");
      content.focus();
      content.getSelection().selectAllChildren(element);
    });

    let sourceTab = await openViewPartialSource("#other", frameBC);

    let browser = gBrowser.selectedBrowser;
    let textContent = await SpecialPowers.spawn(browser, [], async function() {
      return content.document.body.textContent;
    });
    is(
      textContent,
      '<a id="other" href="about:about">other text</a>',
      "Correct content loaded"
    );
    let selection = await SpecialPowers.spawn(browser, [], async function() {
      return String(content.getSelection());
    });
    is(selection, "other text", "Correct text selected");

    gBrowser.removeTab(sourceTab);
    gBrowser.removeTab(tab);
  }
});
