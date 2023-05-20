/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_file_opening() {
  // Get a ref to the pdf we want to open.
  let dirFileObj = getChromeDir(getResolvedURI(gTestPath));
  dirFileObj.append("file_pdfjs_test.pdf");

  // Change the defaults.
  var oldAction = changeMimeHandler(Ci.nsIHandlerInfo.useSystemDefault, true);

  // Test: "Open with" dialog should not come up, despite pdf.js not being
  // the default - because files from disk should always use pdfjs, unless
  // it is forcibly disabled.
  let openedWindow = false;
  let windowOpenedPromise = new Promise((resolve, reject) => {
    addWindowListener(
      "chrome://mozapps/content/downloads/unknownContentType.xhtml",
      () => {
        openedWindow = true;
        resolve();
      }
    );
  });

  // Open the tab with a system principal:
  var tab = BrowserTestUtils.addTab(gBrowser, dirFileObj.path);

  let pdfjsLoadedPromise = TestUtils.waitForCondition(() => {
    let { contentPrincipal } = tab.linkedBrowser;
    return (contentPrincipal?.URI?.spec || "").endsWith("viewer.html");
  });
  await Promise.race([pdfjsLoadedPromise, windowOpenedPromise]);
  ok(!openedWindow, "Shouldn't open an unknownContentType window!");

  BrowserTestUtils.removeTab(tab);

  // Now try opening it from the file directory:
  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    dirFileObj.parent.path
  );
  pdfjsLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    url => url.endsWith("test.pdf")
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.querySelector("a[href$='test.pdf']").click();
  });
  await Promise.race([pdfjsLoadedPromise, windowOpenedPromise]);
  ok(
    !openedWindow,
    "Shouldn't open an unknownContentType window for PDFs from file: links!"
  );

  registerCleanupFunction(function () {
    if (listenerCleanup) {
      listenerCleanup();
    }
    changeMimeHandler(oldAction[0], oldAction[1]);
    gBrowser.removeTab(tab);
  });
});

let listenerCleanup;
function addWindowListener(aURL, aCallback) {
  let listener = {
    onOpenWindow(aXULWindow) {
      info("window opened, waiting for focus");
      listenerCleanup();
      listenerCleanup = null;

      var domwindow = aXULWindow.docShell.domWindow;
      waitForFocus(function () {
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
  };
  Services.wm.addListener(listener);
  listenerCleanup = () => Services.wm.removeListener(listener);
}
