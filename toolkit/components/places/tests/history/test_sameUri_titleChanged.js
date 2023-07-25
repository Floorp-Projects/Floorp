// Test that repeated additions of the same URI to history, properly
// update from_visit and notify titleChanged.

add_task(async function test() {
  let uri = "http://test.com/";

  const promiseTitleChangedNotifications =
    PlacesTestUtils.waitForNotification("page-title-changed");

  // This repeats the url on purpose, don't merge it into a single place entry.
  await PlacesTestUtils.addVisits([
    { uri, title: "test" },
    { uri, referrer: uri, title: "test2" },
  ]);

  const events = await promiseTitleChangedNotifications;
  Assert.equal(events.length, 1, "Right number of title changed notified");
  Assert.equal(events[0].url, uri, "Should notify the proper url");

  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.uri = NetUtil.newURI(uri);
  options.resultType = options.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, 2);

  let child = root.getChild(0);
  Assert.equal(
    child.visitType,
    TRANSITION_LINK,
    "Visit type should be TRANSITION_LINK"
  );
  Assert.equal(child.visitId, 1, "Visit ID should be 1");
  Assert.equal(child.title, "test2", "Should have the correct title");

  child = root.getChild(1);
  Assert.equal(
    child.visitType,
    TRANSITION_LINK,
    "Visit type should be TRANSITION_LINK"
  );
  Assert.equal(child.visitId, 2, "Visit ID should be 2");
  Assert.equal(child.title, "test2", "Should have the correct title");

  root.containerOpen = false;
});
