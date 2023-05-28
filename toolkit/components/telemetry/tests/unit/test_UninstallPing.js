/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { BasePromiseWorker } = ChromeUtils.importESModule(
  "resource://gre/modules/PromiseWorker.sys.mjs"
);
const { TelemetryStorage } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryStorage.sys.mjs"
);
const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);

const gFakeInstallPathHash = "0123456789ABCDEF";
let gFakeVendorDirectory;
let gFakeGetUninstallPingPath;

add_setup(async function setup() {
  do_get_profile();

  let fakeVendorDirectoryNSFile = new FileUtils.File(
    PathUtils.join(PathUtils.profileDir, "uninstall-ping-test")
  );
  fakeVendorDirectoryNSFile.createUnique(
    Ci.nsIFile.DIRECTORY_TYPE,
    FileUtils.PERMS_DIRECTORY
  );
  gFakeVendorDirectory = fakeVendorDirectoryNSFile.path;

  gFakeGetUninstallPingPath = id => ({
    directory: fakeVendorDirectoryNSFile.clone(),
    file: `uninstall_ping_${gFakeInstallPathHash}_${id}.json`,
  });

  fakeUninstallPingPath(gFakeGetUninstallPingPath);

  registerCleanupFunction(async () => {
    await IOUtils.remove(gFakeVendorDirectory, { recursive: true });
  });
});

function ping_path(ping) {
  let { directory: pingFile, file } = gFakeGetUninstallPingPath(ping.id);
  pingFile.append(file);
  return pingFile.path;
}

add_task(async function test_store_ping() {
  // Remove shouldn't throw on an empty dir.
  await TelemetryStorage.removeUninstallPings();

  // Write ping
  const ping1 = {
    id: "58b63aac-999e-4efb-9d5a-20f368670721",
    payload: { some: "thing" },
  };
  const ping1Path = ping_path(ping1);
  await TelemetryStorage.saveUninstallPing(ping1);

  // Check the ping
  Assert.ok(await IOUtils.exists(ping1Path));
  const readPing1 = await IOUtils.readJSON(ping1Path);
  Assert.deepEqual(ping1, readPing1);

  // Write another file that shouldn't match the pattern
  const otherFilePath = PathUtils.join(gFakeVendorDirectory, "other_file.json");
  await IOUtils.writeUTF8(otherFilePath, "");
  Assert.ok(await IOUtils.exists(otherFilePath));

  // Write another ping, should remove the earlier one
  const ping2 = {
    id: "7202c564-8f23-41b4-8a50-1744e9549260",
    payload: { another: "thing" },
  };
  const ping2Path = ping_path(ping2);
  await TelemetryStorage.saveUninstallPing(ping2);

  Assert.ok(!(await IOUtils.exists(ping1Path)));
  Assert.ok(await IOUtils.exists(ping2Path));
  Assert.ok(await IOUtils.exists(otherFilePath));

  // Write an additional file manually so there are multiple matching pings to remove
  const ping3 = { id: "yada-yada" };
  const ping3Path = ping_path(ping3);

  await IOUtils.writeUTF8(ping3Path, "");
  Assert.ok(await IOUtils.exists(ping3Path));

  // Remove pings
  await TelemetryStorage.removeUninstallPings();

  // Check our pings are removed but other file isn't
  Assert.ok(!(await IOUtils.exists(ping1Path)));
  Assert.ok(!(await IOUtils.exists(ping2Path)));
  Assert.ok(!(await IOUtils.exists(ping3Path)));
  Assert.ok(await IOUtils.exists(otherFilePath));

  // Remove again, confirming that the remove doesn't cause an error if nothing to remove
  await TelemetryStorage.removeUninstallPings();

  const ping4 = {
    id: "1f113673-753c-4fbe-9143-fe197f936036",
    payload: { any: "thing" },
  };
  const ping4Path = ping_path(ping4);
  await TelemetryStorage.saveUninstallPing(ping4);

  // Use a worker to keep the ping file open, so a delete should fail.
  const worker = new BasePromiseWorker(
    "resource://test/file_UninstallPing.worker.js"
  );
  await worker.post("open", [ping4Path]);

  // Check that there is no error if the file can't be removed.
  await TelemetryStorage.removeUninstallPings();

  // And file should still exist.
  Assert.ok(await IOUtils.exists(ping4Path));

  // Close the file, so it should be possible to remove now.
  await worker.post("close");
  await TelemetryStorage.removeUninstallPings();
  Assert.ok(!(await IOUtils.exists(ping4Path)));
});
