/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { AsyncShutdown } = ChromeUtils.importESModule(
  "resource://gre/modules/AsyncShutdown.sys.mjs"
);

const { FirefoxAdapter } = ChromeUtils.importESModule(
  "resource://services-common/kinto-storage-adapter.sys.mjs"
);

add_task(async function test_sqlite_shutdown() {
  const sqliteHandle = await FirefoxAdapter.openConnection({
    path: "kinto.sqlite",
  });

  // Shutdown Sqlite.sys.mjs synchronously.
  Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
  AsyncShutdown.profileBeforeChange._trigger();
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");

  try {
    sqliteHandle.execute("SELECT 1;");
    equal("Should not succeed, connection should be closed.", false);
  } catch (e) {
    equal(e.message, "Connection is not open.");
  }
});
