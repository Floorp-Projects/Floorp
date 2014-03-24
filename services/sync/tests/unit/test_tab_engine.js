/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function getMocks() {
  let engine = new TabEngine(Service);
  let store = engine._store;
  store.getTabState = mockGetTabState;
  store.shouldSkipWindow = mockShouldSkipWindow;
  return [engine, store];
}

function run_test() {
  _("Test getOpenURLs.");
  let [engine, store] = getMocks();

  let urls = ["http://bar.com", "http://foo.com", "http://foobar.com"];
  function threeURLs() {
    return urls.pop();
  }
  store.getWindowEnumerator = mockGetWindowEnumerator.bind(this, threeURLs, 1, 3);

  let matches;

  _("  test matching works (true)");
  let openurlsset = engine.getOpenURLs();
  matches = openurlsset.has("http://foo.com");
  do_check_true(matches);

  _("  test matching works (false)");
  matches = openurlsset.has("http://barfoo.com");
  do_check_false(matches);
}
