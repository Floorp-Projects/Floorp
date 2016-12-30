/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Start by getting an empty directory.
var dir = do_get_profile();
dir.append("temp");
dir.create(dir.DIRECTORY_TYPE, -1);
var path = dir.path + "/";

// Now create some sample entries.
var file = dir.clone();
file.append("test_file");
file.create(file.NORMAL_FILE_TYPE, -1);
file = dir.clone();
file.append("other_file");
file.create(file.NORMAL_FILE_TYPE, -1);
dir.append("test_dir");
dir.create(dir.DIRECTORY_TYPE, -1);

var gListener = {
  onSearchResult(aSearch, aResult) {
    // Check that we got same search string back.
    do_check_eq(aResult.searchString, "test");
    // Check that the search succeeded.
    do_check_eq(aResult.searchResult, aResult.RESULT_SUCCESS);
    // Check that we got two results.
    do_check_eq(aResult.matchCount, 2);
    // Check that the first result is the directory we created.
    do_check_eq(aResult.getValueAt(0), "test_dir");
    // Check that the first result has directory style.
    do_check_eq(aResult.getStyleAt(0), "directory");
    // Check that the second result is the file we created.
    do_check_eq(aResult.getValueAt(1), "test_file");
    // Check that the second result has file style.
    do_check_eq(aResult.getStyleAt(1), "file");
  }
};

function run_test()
{
  Components.classes["@mozilla.org/autocomplete/search;1?name=file"]
            .getService(Components.interfaces.nsIAutoCompleteSearch)
            .startSearch("test", path, null, gListener);
}
