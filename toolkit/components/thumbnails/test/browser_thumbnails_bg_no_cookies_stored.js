/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// check that if a page captured in the background attempts to set a cookie,
// that cookie is not saved for subsequent requests.
function* runTests() {
  let url = bgTestPageURL({
    setRedCookie: true,
    iframe: bgTestPageURL({ setRedCookie: true}),
    xhr: bgTestPageURL({ setRedCookie: true})
  });
  ok(!thumbnailExists(url), "Thumbnail file should not exist before capture.");
  yield bgCapture(url);
  ok(thumbnailExists(url), "Thumbnail file should exist after capture.");
  removeThumbnail(url);
  // now load it up in a browser - it should *not* be red, otherwise the
  // cookie above was saved.
  let tab = gBrowser.loadOneTab(url, { inBackground: false });
  let browser = tab.linkedBrowser;
  yield whenLoaded(browser);

  // The root element of the page shouldn't be red.
  let redStr = "rgb(255, 0, 0)";
  isnot(browser.contentDocument.documentElement.style.backgroundColor,
        redStr,
        "The page shouldn't be red.");
  gBrowser.removeTab(tab);
}
