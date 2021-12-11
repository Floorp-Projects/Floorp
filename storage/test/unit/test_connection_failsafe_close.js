/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file tests edge-cases related to mozStorageService::unregisterConnection
 * in the face of failsafe closing at destruction time which results in
 * SpinningSynchronousClose being invoked which can "resurrect" the connection
 * and result in a second call to unregisterConnection.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1413501 for more context.
 */

add_task(async function test_failsafe_close_of_async_connection() {
  // get the db
  let db = getOpenedDatabase();

  // do something async
  let callbackInvoked = new Promise(resolve => {
    db.executeSimpleSQLAsync("CREATE TABLE test (id INTEGER)", {
      handleCompletion: resolve,
    });
  });

  // drop our reference and force a GC so the only live reference is owned by
  // the async statement.
  db = gDBConn = null;
  // (we don't need to cycle collect)
  Cu.forceGC();

  // now we need to wait for that callback to have completed.
  await callbackInvoked;

  Assert.ok(true, "if we shutdown cleanly and do not crash, then we succeeded");
});
