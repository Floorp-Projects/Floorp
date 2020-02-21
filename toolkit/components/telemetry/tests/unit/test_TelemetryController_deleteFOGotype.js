/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);

add_task(async function testDeleteFOGotypeDirs() {
  const tmpdir = OS.Constants.Path.tmpDir;
  const pids = [424242, 535353];
  const dirs = pids.map(pid => OS.Path.join(tmpdir, `fogotype.${pid}`));

  for (let dir of dirs) {
    await OS.File.makeDir(dir, { ignoreExisting: true });
  }

  for (let dir of dirs) {
    Assert.ok(
      await OS.File.exists(dir),
      "Created FOGotype directories should exist to be deleted."
    );
  }

  await TelemetryController.testDeleteFOGotypeDirs();

  for (let dir of dirs) {
    Assert.ok(
      !(await OS.File.exists(dir)),
      "FOGotype directories should now be deleted."
    );
  }
});
