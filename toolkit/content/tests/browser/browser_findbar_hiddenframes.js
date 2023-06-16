const TEST_PAGE = "https://example.com/document-builder.sjs?html=";

let content =
  "<html><body><iframe id='a' src='data:text/html,This is the first page'></iframe><iframe id='b' src='data:text/html,That is another page'></iframe></body></html>";

async function doAndCheckFind(bc, text) {
  await promiseFindFinished(gBrowser, text, false);

  let foundText = await SpecialPowers.spawn(bc, [], () => {
    return content.getSelection().toString();
  });
  is(foundText, text, text + " is found");
}

// This test verifies that find continues to work when a find begins and the frame
// is hidden during the next find step.
add_task(async function test_frame() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE + content
  );
  let browser = gBrowser.getBrowserForTab(tab);

  let findbar = await gBrowser.getFindBar();

  await doAndCheckFind(browser.browsingContext.children[0], "This");

  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("a").style.display = "none";
    content.document.getElementById("a").getBoundingClientRect(); // flush
  });

  await doAndCheckFind(browser.browsingContext.children[1], "another");

  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("a").style.display = "";
    content.document.getElementById("a").getBoundingClientRect();
  });

  await doAndCheckFind(browser.browsingContext.children[0], "first");

  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("a").style.visibility = "hidden";
    content.document.getElementById("a").getBoundingClientRect();
  });

  await doAndCheckFind(browser.browsingContext.children[1], "That");

  await SpecialPowers.spawn(browser, [], () => {
    content.document.getElementById("a").style.visibility = "";
    content.document.getElementById("a").getBoundingClientRect();
  });

  await doAndCheckFind(browser.browsingContext.children[0], "This");

  await closeFindbarAndWait(findbar);

  gBrowser.removeTab(tab);
});
