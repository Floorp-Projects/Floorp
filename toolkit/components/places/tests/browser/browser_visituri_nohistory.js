/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const INITIAL_URL = "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
const FINAL_URL = "http://example.com/tests/toolkit/components/places/tests/browser/final.html";

/**
 * One-time observer callback.
 */
function promiseObserve(name)
{
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject) {
      Services.obs.removeObserver(observer, name);
      resolve(subject);
    }, name, false);
  });
}

add_task(function* ()
{
  yield SpecialPowers.pushPrefEnv({"set": [["places.history.enabled", false]]});

  let visitUriPromise = promiseObserve("uri-visit-saved");

  yield BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);

  yield SpecialPowers.popPrefEnv();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.loadURI(FINAL_URL);
  yield browserLoadedPromise;

  let subject = yield visitUriPromise;
  let uri = subject.QueryInterface(Ci.nsIURI);
  is(uri.spec, FINAL_URL, "received expected visit");

  yield PlacesTestUtils.clearHistory();
  gBrowser.removeCurrentTab();
});
