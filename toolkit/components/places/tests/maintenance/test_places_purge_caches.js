/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test whether purge-caches event works collectry when maintenance the places.

add_task(async function test_history() {
  await PlacesTestUtils.addVisits({ uri: "http://example.com/" });
  await assertPurgingCaches();
});

add_task(async function test_bookmark() {
  await PlacesTestUtils.addBookmarkWithDetails({ uri: "http://example.com/" });
  await assertPurgingCaches();
});

async function assertPurgingCaches() {
  const query = PlacesUtils.history.getNewQuery();
  const options = PlacesUtils.history.getNewQueryOptions();
  const result = PlacesUtils.history.executeQuery(query, options);
  result.root.containerOpen = true;

  const onInvalidateContainer = new Promise(resolve => {
    const resultObserver = new NavHistoryResultObserver();
    resultObserver.invalidateContainer = resolve;
    result.addObserver(resultObserver, false);
  });

  await PlacesDBUtils.maintenanceOnIdle();
  await onInvalidateContainer;
  ok(true, "InvalidateContainer is called");
}
