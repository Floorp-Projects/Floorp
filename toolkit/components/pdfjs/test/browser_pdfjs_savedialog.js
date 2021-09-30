/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

function test() {
  // When the download panel improvements pref is true, we expect the PDF to be simply downloaded
  // So it only makes sense to run this test with the pref set false
  Services.prefs.setBoolPref(
    "browser.download.improvements_to_download_panel",
    false
  );
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);
  var tab = BrowserTestUtils.addTab(gBrowser, TESTROOT + "file_pdfjs_test.pdf");

  // Test: "Open with" dialog comes up when pdf.js is not selected as the default
  // handler.
  addWindowListener(
    "chrome://mozapps/content/downloads/unknownContentType.xhtml",
    finish
  );

  waitForExplicitFinish();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(
      "browser.download.improvements_to_download_panel"
    );
    changeMimeHandler(oldAction[0], oldAction[1]);
    gBrowser.removeTab(tab);
  });
}

function addWindowListener(aURL, aCallback) {
  Services.wm.addListener({
    onOpenWindow(aXULWindow) {
      info("window opened, waiting for focus");
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.docShell.domWindow;
      waitForFocus(function() {
        is(
          domwindow.document.location.href,
          aURL,
          "should have seen the right window open"
        );
        domwindow.close();
        aCallback();
      }, domwindow);
    },
    onCloseWindow(aXULWindow) {},
  });
}
