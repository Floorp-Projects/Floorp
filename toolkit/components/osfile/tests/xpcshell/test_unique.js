"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

function run_test() {
  do_get_profile();
  run_next_test();
}

function testFiles(filename) {
  return Task.spawn(function() {
    const MAX_TRIES = 10;
    let profileDir = OS.Constants.Path.profileDir;
    let path = OS.Path.join(profileDir, filename);

    // Ensure that openUnique() uses the file name if there is no file with that name already.
    let openedFile = yield OS.File.openUnique(path);
    do_print("\nCreate new file: " + openedFile.path);
    yield openedFile.file.close();
    let exists = yield OS.File.exists(openedFile.path);
    do_check_true(exists);
    do_check_eq(path, openedFile.path);
    let fileInfo = yield OS.File.stat(openedFile.path);
    do_check_true(fileInfo.size == 0);

    // Ensure that openUnique() creates a new file name using a HEX number, as the original name is already taken.
    openedFile = yield OS.File.openUnique(path);
    do_print("\nCreate unique HEX file: " + openedFile.path);
    yield openedFile.file.close();
    exists = yield OS.File.exists(openedFile.path);
    do_check_true(exists);
    fileInfo = yield OS.File.stat(openedFile.path);
    do_check_true(fileInfo.size == 0);

    // Ensure that openUnique() generates different file names each time, using the HEX number algorithm
    let filenames = new Set();
    for (let i=0; i < MAX_TRIES; i++) {
      openedFile = yield OS.File.openUnique(path);
      yield openedFile.file.close();
      filenames.add(openedFile.path);
    }

    do_check_eq(filenames.size, MAX_TRIES);

    // Ensure that openUnique() creates a new human readable file name using, as the original name is already taken.
    openedFile = yield OS.File.openUnique(path, {humanReadable : true});
    do_print("\nCreate unique Human Readable file: " + openedFile.path);
    yield openedFile.file.close();
    exists = yield OS.File.exists(openedFile.path);
    do_check_true(exists);
    fileInfo = yield OS.File.stat(openedFile.path);
    do_check_true(fileInfo.size == 0);

    // Ensure that openUnique() generates different human readable file names each time
    filenames = new Set();
    for (let i=0; i < MAX_TRIES; i++) {
      openedFile = yield OS.File.openUnique(path, {humanReadable : true});
      yield openedFile.file.close();
      filenames.add(openedFile.path);
    }

    do_check_eq(filenames.size, MAX_TRIES);

    let exn;
    try {
      for (let i=0; i < 100; i++) {
        openedFile = yield OS.File.openUnique(path, {humanReadable : true});
        yield openedFile.file.close();
      }
    } catch (ex) {
      exn = ex;
    }

    do_print("Ensure that this raises the correct error");
    do_check_true(!!exn);
    do_check_true(exn instanceof OS.File.Error);
    do_check_true(exn.becauseExists);
  });
}

add_task(function test_unique() {
  OS.Shared.DEBUG = true;
  // Tests files with extension
  yield testFiles("dummy_unique_file.txt");
  // Tests files with no extension
  yield testFiles("dummy_unique_file_no_ext");
});
