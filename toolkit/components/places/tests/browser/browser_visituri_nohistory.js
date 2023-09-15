/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const INITIAL_URL =
  "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
const FINAL_URL =
  "http://test1.example.com/tests/toolkit/components/places/tests/browser/final.html";

/**
 * One-time observer callback.
 */
function promiseObserve(name) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject) {
      Services.obs.removeObserver(observer, name);
      resolve(subject);
    }, name);
  });
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({ set: [["places.history.enabled", false]] });

  let visitUriPromise = promiseObserve("uri-visit-saved");

  await BrowserTestUtils.openNewForegroundTab(gBrowser, INITIAL_URL);

  await SpecialPowers.popPrefEnv();

  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, FINAL_URL);
  await browserLoadedPromise;

  let subject = await visitUriPromise;
  let uri = subject.QueryInterface(Ci.nsIURI);
  is(uri.spec, FINAL_URL, "received expected visit");

  await PlacesUtils.history.clear();
  gBrowser.removeCurrentTab();
});
