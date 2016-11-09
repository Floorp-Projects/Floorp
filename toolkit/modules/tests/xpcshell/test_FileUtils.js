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
  do_check_throws(function() {
    FileUtils.getFile("NonexistentD", ["foobar"]);
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
  do_check_throws(function() {
    FileUtils.getDir("NonexistentD", ["foodir"]);
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

var openFileOutputStream_defaultFlags = function(aKind, aFileName) {
  let file = FileUtils.getFile("ProfD", [aFileName]);
  let fos;
  do_check_true(aKind == "atomic" || aKind == "safe" || aKind == "");
  if (aKind == "atomic") {
    fos = FileUtils.openAtomicFileOutputStream(file);
  } else if (aKind == "safe") {
    fos = FileUtils.openSafeFileOutputStream(file);
  } else {
    fos = FileUtils.openFileOutputStream(file);
  }
  do_check_true(fos instanceof Components.interfaces.nsIFileOutputStream);
  if (aKind == "atomic" || aKind == "safe") {
    do_check_true(fos instanceof Components.interfaces.nsISafeOutputStream);
  }

  // FileUtils.openFileOutputStream or FileUtils.openAtomicFileOutputStream()
  // or FileUtils.openSafeFileOutputStream() opens the stream with DEFER_OPEN
  // which means the file will not be open until we write to it.
  do_check_false(file.exists());

  let data = "test_default_flags";
  fos.write(data, data.length);
  do_check_true(file.exists());

  // No nsIXULRuntime in xpcshell, so use this trick to determine whether we're
  // on Windows.
  if ("@mozilla.org/windows-registry-key;1" in Components.classes) {
    do_check_eq(file.permissions, 0o666);
  } else {
    do_check_eq(file.permissions, FileUtils.PERMS_FILE);
  }

  run_next_test();
};

var openFileOutputStream_modeFlags = function(aKind, aFileName) {
  let file = FileUtils.getFile("ProfD", [aFileName]);
  let fos;
  do_check_true(aKind == "atomic" || aKind == "safe" || aKind == "");
  if (aKind == "atomic") {
    fos = FileUtils.openAtomicFileOutputStream(file, FileUtils.MODE_WRONLY);
  } else if (aKind == "safe") {
    fos = FileUtils.openSafeFileOutputStream(file, FileUtils.MODE_WRONLY);
  } else {
    fos = FileUtils.openFileOutputStream(file, FileUtils.MODE_WRONLY);
  }
  let data = "test_modeFlags";
  do_check_throws(function() {
    fos.write(data, data.length);
  }, Components.results.NS_ERROR_FILE_NOT_FOUND);
  do_check_false(file.exists());

  run_next_test();
};

var closeFileOutputStream = function(aKind, aFileName) {
  let file = FileUtils.getFile("ProfD", [aFileName]);
  let fos;
  do_check_true(aKind == "atomic" || aKind == "safe");
  if (aKind == "atomic") {
    fos = FileUtils.openAtomicFileOutputStream(file);
  } else if (aKind == "safe") {
    fos = FileUtils.openSafeFileOutputStream(file);
  }

  // We can write data to the stream just fine while it's open.
  let data = "testClose";
  fos.write(data, data.length);

  // But once we close it, we can't anymore.
  if (aKind == "atomic") {
    FileUtils.closeAtomicFileOutputStream(fos);
  } else if (aKind == "safe") {
    FileUtils.closeSafeFileOutputStream(fos);
  }
  do_check_throws(function() {
    fos.write(data, data.length);
  }, Components.results.NS_BASE_STREAM_CLOSED);
  run_next_test();
};

add_test(function test_openFileOutputStream_defaultFlags() {
  openFileOutputStream_defaultFlags("", "george");
});

// openFileOutputStream will uses MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE
// as the default mode flags, but we can pass in our own if we want to.
add_test(function test_openFileOutputStream_modeFlags() {
  openFileOutputStream_modeFlags("", "ringo");
});

add_test(function test_openAtomicFileOutputStream_defaultFlags() {
  openFileOutputStream_defaultFlags("atomic", "peiyong");
});

// openAtomicFileOutputStream will uses MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE
// as the default mode flags, but we can pass in our own if we want to.
add_test(function test_openAtomicFileOutputStream_modeFlags() {
  openFileOutputStream_modeFlags("atomic", "lin");
});

add_test(function test_closeAtomicFileOutputStream() {
  closeFileOutputStream("atomic", "peiyonglin");
});

add_test(function test_openSafeFileOutputStream_defaultFlags() {
  openFileOutputStream_defaultFlags("safe", "john");
});

// openSafeFileOutputStream will uses MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE
// as the default mode flags, but we can pass in our own if we want to.
add_test(function test_openSafeFileOutputStream_modeFlags() {
  openFileOutputStream_modeFlags("safe", "paul");
});

add_test(function test_closeSafeFileOutputStream() {
  closeFileOutputStream("safe", "georgee");
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
