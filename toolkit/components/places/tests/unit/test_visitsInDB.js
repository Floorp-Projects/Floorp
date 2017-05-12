/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

add_task(async function test_execute() {
  const TEST_URI = uri("http://mozilla.com");

  do_check_eq(0, await PlacesTestUtils.visitsInDB(TEST_URI));
  await PlacesTestUtils.addVisits({uri: TEST_URI});
  do_check_eq(1, await PlacesTestUtils.visitsInDB(TEST_URI));
  await PlacesTestUtils.addVisits({uri: TEST_URI});
  do_check_eq(2, await PlacesTestUtils.visitsInDB(TEST_URI));
});
