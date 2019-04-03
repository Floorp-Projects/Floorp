/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This file tests that invoking `Service::minimizeMemory` succeeds for sync
// and async connections.

function minimizeMemory() {
  Services.storage.QueryInterface(Ci.nsIObserver)
                  .observe(null, "memory-pressure", null);
}

add_task(async function test_minimizeMemory_async_connection() {
  let db = await openAsyncDatabase(getTestDB());
  minimizeMemory();
  await asyncClose(db);
});

add_task(async function test_minimizeMemory_sync_connection() {
  let db = getOpenedDatabase();
  minimizeMemory();
  db.close();
});
