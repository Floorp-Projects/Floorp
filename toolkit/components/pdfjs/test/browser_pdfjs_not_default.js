/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_pdfjs_not_default() {
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);
  let dirFileObj = getChromeDir(getResolvedURI(gTestPath));
  dirFileObj.append("file_pdfjs_test.pdf");

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    dirFileObj.path
  );

  // If we don't have the Pdfjs actor loaded, this will throw
  await getPdfjsActor();

  changeMimeHandler(oldAction[0], oldAction[1]);

  gBrowser.removeTab(tab);
});

function getPdfjsActor() {
  let win = Services.wm.getMostRecentWindow("navigator:browser");
  let selectedBrowser = win.gBrowser.selectedBrowser;
  return selectedBrowser.browsingContext.currentWindowGlobal.getActor("Pdfjs");
}
