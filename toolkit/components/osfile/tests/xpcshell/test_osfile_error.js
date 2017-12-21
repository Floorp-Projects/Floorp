/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {OS: {File, Path, Constants}} = Components.utils.import("resource://gre/modules/osfile.jsm", {});

add_task(async function testFileError_with_writeAtomic() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "testFileError.tmp");
  await File.remove(path);
  await File.writeAtomic(path, DEFAULT_CONTENTS);
  let exception;
  try {
    await File.writeAtomic(path, DEFAULT_CONTENTS, { noOverwrite: true });
  } catch (ex) {
    exception = ex;
  }
  Assert.ok(exception instanceof File.Error);
  Assert.ok(exception.path == path);
});

add_task(async function testFileError_with_makeDir() {
  let path = Path.join(Constants.Path.tmpDir,
                       "directory");
  await File.removeDir(path);
  await File.makeDir(path);
  let exception;
  try {
    await File.makeDir(path, { ignoreExisting: false });
  } catch (ex) {
    exception = ex;
  }
  Assert.ok(exception instanceof File.Error);
  Assert.ok(exception.path == path);
});

add_task(async function testFileError_with_move() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let sourcePath = Path.join(Constants.Path.tmpDir,
                             "src.tmp");
  let destPath = Path.join(Constants.Path.tmpDir,
                           "dest.tmp");
  await File.remove(sourcePath);
  await File.remove(destPath);
  await File.writeAtomic(sourcePath, DEFAULT_CONTENTS);
  await File.writeAtomic(destPath, DEFAULT_CONTENTS);
  let exception;
  try {
    await File.move(sourcePath, destPath, { noOverwrite: true });
  } catch (ex) {
    exception = ex;
  }
  info(exception);
  Assert.ok(exception instanceof File.Error);
  Assert.ok(exception.path == sourcePath);
});
