/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var aaaListener = {
  onSearchResult(search, result) {
    do_check_eq(result.searchString, "aaa");
    do_test_finished();
  }
};

var aaListener = {
  onSearchResult(search, result) {
    do_check_eq(result.searchString, "aa");
    search.startSearch("aaa", "", result, aaaListener);
  }
};

function run_test() {
  do_test_pending();
  let search = Cc["@mozilla.org/autocomplete/search;1?name=form-history"].
               getService(Ci.nsIAutoCompleteSearch);
  search.startSearch("aa", "", null, aaListener);
}
