/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Migrations between 1 and 2 discard the entire database.
add_task(async function test_migrate_from_1_to_2() {
  let dbFile = do_get_file("mirror_v1.sqlite");
  let telemetryEvents = [];
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    recordTelemetryEvent(object, method, value, extra) {
      telemetryEvents.push({ object, method, value, extra });
    },
  });
  deepEqual(telemetryEvents, [{
    object: "mirror",
    method: "open",
    value: "retry",
    extra: { why: "corrupt" },
  }]);
  await buf.finalize();
});

add_task(async function test_database_corrupt() {
  let corruptFile = do_get_file("mirror_corrupt.sqlite");
  let telemetryEvents = [];
  let buf = await SyncedBookmarksMirror.open({
    path: corruptFile.path,
    recordTelemetryEvent(object, method, value, extra) {
      telemetryEvents.push({ object, method, value, extra });
    },
  });
  deepEqual(telemetryEvents, [{
    object: "mirror",
    method: "open",
    value: "retry",
    extra: { why: "corrupt" },
  }]);
  await buf.finalize();
});

add_task(async function test_database_readonly() {
  let dbFile = do_get_file("mirror_v1.sqlite");
  try {
    await OS.File.setPermissions(dbFile.path, {
      unixMode: 0o400,
      winAttributes: { readOnly: true },
    });
  } catch (ex) {
    if (ex instanceof OS.File.Error &&
        ex.unixErrno == OS.Constants.libc.EPERM) {
      info("Skipping test; can't change database mode to read-only");
      return;
    }
    throw ex;
  }
  try {
    let telemetryEvents = [];
    await Assert.rejects(SyncedBookmarksMirror.open({
      path: dbFile.path,
      recordTelemetryEvent(object, method, value, extra) {
        telemetryEvents.push({ object, method, value, extra });
      },
    }), ex => ex.errors && ex.errors[0].result == Ci.mozIStorageError.READONLY,
      "Should not try to recreate read-only database");
    deepEqual(telemetryEvents, [{
      object: "mirror",
      method: "open",
      value: "error",
      extra: { why: "initialize" },
    }], "Should record event for read-only error");
  } finally {
    await OS.File.setPermissions(dbFile.path, {
      unixMode: 0o644,
      winAttributes: { readOnly: false },
    });
  }
});
