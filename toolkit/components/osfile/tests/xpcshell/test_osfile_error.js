/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {OS: {File, Path, Constants}} = Components.utils.import("resource://gre/modules/osfile.jsm", {});
Components.utils.import("resource://gre/modules/Task.jsm");

function run_test() {
  run_next_test();
}

add_task(function* testFileError_with_writeAtomic() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "testFileError.tmp");
  yield File.remove(path);
  yield File.writeAtomic(path, DEFAULT_CONTENTS);
  let exception;
  try {
    yield File.writeAtomic(path, DEFAULT_CONTENTS, { noOverwrite: true });
  } catch (ex) {
    exception = ex;
  }
  do_check_true(exception instanceof File.Error);
  do_check_true(exception.path == path);
});

add_task(function* testFileError_with_makeDir() {
  let path = Path.join(Constants.Path.tmpDir,
                       "directory");
  yield File.removeDir(path);
  yield File.makeDir(path);
  let exception;
  try {
    yield File.makeDir(path, { ignoreExisting: false });
  } catch (ex) {
    exception = ex;
  }
  do_check_true(exception instanceof File.Error);
  do_check_true(exception.path == path);
});

add_task(function* testFileError_with_move() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let sourcePath = Path.join(Constants.Path.tmpDir,
                             "src.tmp");
  let destPath = Path.join(Constants.Path.tmpDir,
                           "dest.tmp");
  yield File.remove(sourcePath);
  yield File.remove(destPath);
  yield File.writeAtomic(sourcePath, DEFAULT_CONTENTS);
  yield File.writeAtomic(destPath, DEFAULT_CONTENTS);
  let exception;
  try {
    yield File.move(sourcePath, destPath, { noOverwrite: true });
  } catch (ex) {
    exception = ex;
  }
  do_print(exception);
  do_check_true(exception instanceof File.Error);
  do_check_true(exception.path == sourcePath);
});
