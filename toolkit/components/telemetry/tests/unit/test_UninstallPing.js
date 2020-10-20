/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

const gFakeInstallPathHash = "0123456789ABCDEF";
let gFakeVendorDirectory;
let gFakeGetUninstallPingPath;

add_task(async function setup() {
  do_get_profile();

  let fakeVendorDirectoryNSFile = new FileUtils.File(
    OS.Path.join(OS.Constants.Path.profileDir, "uninstall-ping-test")
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

  registerCleanupFunction(() => {
    OS.File.removeDir(gFakeVendorDirectory);
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
  Assert.ok(await OS.File.exists(ping1Path));
  const readPing1 = JSON.parse(
    await OS.File.read(ping1Path, { encoding: "utf-8" })
  );
  Assert.deepEqual(ping1, readPing1);

  // Write another file that shouldn't match the pattern
  const otherFilePath = OS.Path.join(gFakeVendorDirectory, "other_file.json");
  await OS.File.writeAtomic(otherFilePath, "");
  Assert.ok(await OS.File.exists(otherFilePath));

  // Write another ping, should remove the earlier one
  const ping2 = {
    id: "7202c564-8f23-41b4-8a50-1744e9549260",
    payload: { another: "thing" },
  };
  const ping2Path = ping_path(ping2);
  await TelemetryStorage.saveUninstallPing(ping2);

  Assert.ok(!(await OS.File.exists(ping1Path)));
  Assert.ok(await OS.File.exists(ping2Path));
  Assert.ok(await OS.File.exists(otherFilePath));

  // Write an additional file manually so there are multiple matching pings to remove
  const ping3 = { id: "yada-yada" };
  const ping3Path = ping_path(ping3);

  await OS.File.writeAtomic(ping3Path, "");
  Assert.ok(await OS.File.exists(ping3Path));

  // Remove pings
  await TelemetryStorage.removeUninstallPings();

  // Check our pings are removed but other file isn't
  Assert.ok(!(await OS.File.exists(ping1Path)));
  Assert.ok(!(await OS.File.exists(ping2Path)));
  Assert.ok(!(await OS.File.exists(ping3Path)));
  Assert.ok(await OS.File.exists(otherFilePath));

  // Remove again, confirming that the remove doesn't cause an error if nothing to remove
  await TelemetryStorage.removeUninstallPings();

  const ping4 = {
    id: "1f113673-753c-4fbe-9143-fe197f936036",
    payload: { any: "thing" },
  };
  const ping4Path = ping_path(ping4);
  await TelemetryStorage.saveUninstallPing(ping4);

  // Open the ping without FILE_SHARE_DELETE, so a delete should fail.
  const ping4File = await OS.File.open(
    ping4Path,
    { read: true, existing: true },
    { winShare: OS.Constants.Win.FILE_SHARE_READ }
  );

  // Check that there is no error if the file can't be removed.
  await TelemetryStorage.removeUninstallPings();

  // And file should still exist.
  Assert.ok(await OS.File.exists(ping4Path));

  // Close the file, it should be possible to remove now.
  ping4File.close();
  await TelemetryStorage.removeUninstallPings();
  Assert.ok(!(await OS.File.exists(ping4Path)));
});
