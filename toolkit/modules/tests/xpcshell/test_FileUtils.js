/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/FileUtils.jsm");

function do_check_throws(f, result, stack) {
  if (!stack)
    stack = Components.stack.caller;

  try {
    f();
  } catch (exc) {
    if (exc.result == result)
      return;
    do_throw("expected result " + result + ", caught " + exc, stack);
  }
  do_throw("expected result " + result + ", none thrown", stack);
}

const gProfD = do_get_profile();

add_test(function test_getFile() {
  let file = FileUtils.getFile("ProfD", ["foobar"]);
  do_check_true(file instanceof Components.interfaces.nsIFile);
  do_check_false(file.exists());

  let other = gProfD.clone();
  other.append("foobar");
  do_check_true(file.equals(other));

  run_next_test();
});

add_test(function test_getFile_nonexistentDir() {
  do_check_throws(function () {
    let file = FileUtils.getFile("NonexistentD", ["foobar"]);
  }, Components.results.NS_ERROR_FAILURE);

  run_next_test();
});

add_test(function test_getFile_createDirs() {
  let file = FileUtils.getFile("ProfD", ["a", "b", "foobar"]);
  do_check_true(file instanceof Components.interfaces.nsIFile);
  do_check_false(file.exists());

  let other = gProfD.clone();
  other.append("a");
  do_check_true(other.isDirectory());
  other.append("b");
  do_check_true(other.isDirectory());
  other.append("foobar");
  do_check_true(file.equals(other));

  run_next_test();
});

add_test(function test_getDir() {
  let dir = FileUtils.getDir("ProfD", ["foodir"]);
  do_check_true(dir instanceof Components.interfaces.nsIFile);
  do_check_false(dir.exists());

  let other = gProfD.clone();
  other.append("foodir");
  do_check_true(dir.equals(other));

  run_next_test();
});

add_test(function test_getDir_nonexistentDir() {
  do_check_throws(function () {
    let file = FileUtils.getDir("NonexistentD", ["foodir"]);
  }, Components.results.NS_ERROR_FAILURE);

  run_next_test();
});

add_test(function test_getDir_shouldCreate() {
  let dir = FileUtils.getDir("ProfD", ["c", "d", "foodir"], true);
  do_check_true(dir instanceof Components.interfaces.nsIFile);
  do_check_true(dir.exists());

  let other = gProfD.clone();
  other.append("c");
  do_check_true(other.isDirectory());
  other.append("d");
  do_check_true(other.isDirectory());
  other.append("foodir");
  do_check_true(dir.equals(other));

  run_next_test();
});

add_test(function test_openFileOutputStream_defaultFlags() {
  let file = FileUtils.getFile("ProfD", ["george"]);
  let fos = FileUtils.openFileOutputStream(file);
  do_check_true(fos instanceof Components.interfaces.nsIFileOutputStream);

  // FileUtils.openFileOutputStream() opens the stream with DEFER_OPEN
  // which means the file will not be open until we write to it.
  do_check_false(file.exists());

  let data = "imagine";
  fos.write(data, data.length);
  do_check_true(file.exists());

  // No nsIXULRuntime in xpcshell, so use this trick to determine whether we're
  // on Windows.
  if ("@mozilla.org/windows-registry-key;1" in Components.classes) {
    do_check_eq(file.permissions, 0666);
  } else {
    do_check_eq(file.permissions, FileUtils.PERMS_FILE);
  }

  run_next_test();
});

// openFileOutputStream will uses MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE
// as the default mode flags, but we can pass in our own if we want to.
add_test(function test_openFileOutputStream_modeFlags() {
  let file = FileUtils.getFile("ProfD", ["ringo"]);
  let fos = FileUtils.openFileOutputStream(file, FileUtils.MODE_WRONLY);
  let data = "yesterday";
  do_check_throws(function () {
    fos.write(data, data.length);
  }, Components.results.NS_ERROR_FILE_NOT_FOUND);
  do_check_false(file.exists());

  run_next_test();
});

add_test(function test_openSafeFileOutputStream_defaultFlags() {
  let file = FileUtils.getFile("ProfD", ["john"]);
  let fos = FileUtils.openSafeFileOutputStream(file);
  do_check_true(fos instanceof Components.interfaces.nsIFileOutputStream);
  do_check_true(fos instanceof Components.interfaces.nsISafeOutputStream);

  // FileUtils.openSafeFileOutputStream() opens the stream with DEFER_OPEN
  // which means the file will not be open until we write to it.
  do_check_false(file.exists());

  let data = "imagine";
  fos.write(data, data.length);
  do_check_true(file.exists());

  // No nsIXULRuntime in xpcshell, so use this trick to determine whether we're
  // on Windows.
  if ("@mozilla.org/windows-registry-key;1" in Components.classes) {
    do_check_eq(file.permissions, 0666);
  } else {
    do_check_eq(file.permissions, FileUtils.PERMS_FILE);
  }

  run_next_test();
});

// openSafeFileOutputStream will uses MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE
// as the default mode flags, but we can pass in our own if we want to.
add_test(function test_openSafeFileOutputStream_modeFlags() {
  let file = FileUtils.getFile("ProfD", ["paul"]);
  let fos = FileUtils.openSafeFileOutputStream(file, FileUtils.MODE_WRONLY);
  let data = "yesterday";
  do_check_throws(function () {
    fos.write(data, data.length);
  }, Components.results.NS_ERROR_FILE_NOT_FOUND);
  do_check_false(file.exists());

  run_next_test();
});

add_test(function test_closeSafeFileOutputStream() {
  let file = FileUtils.getFile("ProfD", ["george"]);
  let fos = FileUtils.openSafeFileOutputStream(file);

  // We can write data to the stream just fine while it's open.
  let data = "here comes the sun";
  fos.write(data, data.length);

  // But once we close it, we can't anymore.
  FileUtils.closeSafeFileOutputStream(fos);
  do_check_throws(function () {
    fos.write(data, data.length);
  }, Components.results.NS_BASE_STREAM_CLOSED);
  run_next_test();
});

add_test(function test_newFile() {
  let testfile = FileUtils.getFile("ProfD", ["test"]);
  let testpath = testfile.path;
  let file = new FileUtils.File(testpath);
  do_check_true(file instanceof Components.interfaces.nsILocalFile);
  do_check_true(file.equals(testfile));
  do_check_eq(file.path, testpath);
  run_next_test();
});

function run_test() {
  run_next_test();
}
