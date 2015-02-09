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

add_task(function* test_autocomplete_on_value_removed() {
  let listener = Cc["@mozilla.org/autocomplete/search;1?name=unifiedcomplete"].
                 getService(Components.interfaces.nsIAutoCompleteSimpleResultListener);

  let testUri = NetUtil.newURI("http://foo.mozilla.com/");
  yield PlacesTestUtils.addVisits({
    uri: testUri,
    referrer: uri("http://mozilla.com/")
  });

  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  // look for this uri only
  query.uri = testUri;

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  // call the untested code path
  listener.onValueRemoved(null, testUri.spec, true);
  // make sure it is GONE from the DB
  Assert.equal(root.childCount, 0);
  // close the container
  root.containerOpen = false;
});
