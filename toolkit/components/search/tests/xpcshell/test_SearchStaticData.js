/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the SearchStaticData module.
 */

"use strict";

Cu.import("resource://gre/modules/SearchStaticData.jsm", this);

function run_test() {
  do_check_true(SearchStaticData.getAlternateDomains("www.google.com")
                                .indexOf("www.google.fr") != -1);
  do_check_true(SearchStaticData.getAlternateDomains("www.google.fr")
                                .indexOf("www.google.com") != -1);
  do_check_true(SearchStaticData.getAlternateDomains("www.google.com")
                                .every(d => d.startsWith("www.google.")));
  do_check_true(SearchStaticData.getAlternateDomains("google.com").length == 0);

  // Test that methods from SearchStaticData module can be overwritten,
  // needed for hotfixing.
  let backup = SearchStaticData.getAlternateDomains;
  SearchStaticData.getAlternateDomains = () => ["www.bing.fr"];
  do_check_matches(SearchStaticData.getAlternateDomains("www.bing.com"), ["www.bing.fr"]);
  SearchStaticData.getAlternateDomains = backup;
}
