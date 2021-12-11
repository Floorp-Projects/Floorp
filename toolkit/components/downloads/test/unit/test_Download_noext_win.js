/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Get a file without extension");
  let noExtFile = getTempFile("test_bug_1661365");
  Assert.ok(!noExtFile.leafName.includes("."), "Sanity check the filename");
  info("Create an exe file with the same name");
  await IOUtils.remove(noExtFile.path + ".exe", { ignoreAbsent: true });
  let exeFile = new FileUtils.File(noExtFile.path + ".exe");
  exeFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  Assert.ok(await fileExists(exeFile.path), "Sanity check the exe exists.");
  Assert.equal(
    exeFile.leafName,
    noExtFile.leafName + ".exe",
    "Sanity check the file names."
  );
  registerCleanupFunction(async function() {
    await IOUtils.remove(noExtFile.path, { ignoreAbsent: true });
    await IOUtils.remove(exeFile.path, { ignoreAbsent: true });
  });

  info("Download to the no-extension file");
  let download = await Downloads.createDownload({
    source: httpUrl("source.txt"),
    target: noExtFile,
  });
  await download.start();

  Assert.ok(
    await fileExists(download.target.path),
    "The file should have been created."
  );
  Assert.ok(await fileExists(exeFile.path), "Sanity check the exe exists.");

  info("Launch should open the containing folder");
  let promiseShowInFolder = waitForDirectoryShown();
  download.launch();
  Assert.equal(await promiseShowInFolder, noExtFile.path);
});

/**
 * Waits for an attempt to show the directory where a file is located, and
 * returns the path of the file.
 */
function waitForDirectoryShown() {
  return new Promise(resolve => {
    let waitFn = base => ({
      showContainingDirectory(path) {
        Integration.downloads.unregister(waitFn);
        resolve(path);
        return Promise.resolve();
      },
    });
    Integration.downloads.register(waitFn);
  });
}
