/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

add_task(function* test_execute() {
  var good_uri = uri("http://mozilla.com");
  var bad_uri = uri("http://google.com");
  yield PlacesTestUtils.addVisits({uri: good_uri});
  do_check_true(yield PlacesTestUtils.isPageInDB(good_uri));
  do_check_false(yield PlacesTestUtils.isPageInDB(bad_uri));
});
