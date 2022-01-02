/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

add_task(async function test_execute() {
  var good_uri = uri("http://mozilla.com");
  var bad_uri = uri("http://google.com");
  await PlacesTestUtils.addVisits({ uri: good_uri });
  Assert.ok(await PlacesTestUtils.isPageInDB(good_uri));
  Assert.equal(false, await PlacesTestUtils.isPageInDB(bad_uri));
});
