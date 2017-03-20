/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Need to test that removing a page from autocomplete actually removes a page
 * Description From  Shawn Wilsher :sdwilsh   2009-02-18 11:29:06 PST
 * We don't test the code path of onValueRemoved
 * for the autocomplete implementation
 * Bug 479089
 */

function promiseURIDeleted(testURI) {
  return new Promise(resolve => {
    let obs = new NavHistoryObserver();
    obs.onDeleteURI = (uri, guid, reason) => {
      PlacesUtils.history.removeObserver(obs);
      Assert.equal(uri.spec, testURI.spec, "Deleted URI should be the expected one");
      resolve();
    };
    PlacesUtils.history.addObserver(obs, false);
  });
}

add_task(function* test_autocomplete_on_value_removed() {
  let listener = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"].
                 getService(Components.interfaces.nsIAutoCompleteSimpleResultListener);

  let testUri = NetUtil.newURI("http://foo.mozilla.com/");
  yield PlacesTestUtils.addVisits({
    uri: testUri,
    referrer: uri("http://mozilla.com/")
  });

  let promise = promiseURIDeleted(testUri);
  // call the untested code path
  listener.onValueRemoved(null, testUri.spec, true);

  yield promise;
});
