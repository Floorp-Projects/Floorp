/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function log(text) {
  dump("WORKER "+text+"\n");
}

function send(message) {
  self.postMessage(message);
}

self.onmessage = function onmessage_start(msg) {
  self.onmessage = function onmessage_ignored(msg) {
    log("ignored message " + JSON.stringify(msg.data));
  };
  try {
    test_init();
    test_open_existing_file();
    test_open_non_existing_file();
    test_copy_existing_file();
    test_read_write_file();
    test_move_file();
    test_iter_dir();
    test_info();
  } catch (x) {
    log("Catching error: " + x);
    log("Stack: " + x.stack);
    log("Source: " + x.toSource());
    ok(false, x.toString() + "\n" + x.stack);
  }
  finish();
};

function finish() {
  send({kind: "finish"});
}

function ok(condition, description) {
  send({kind: "ok", condition: condition, description:description});
}
function is(a, b, description) {
  send({kind: "is", a: a, b:b, description:description});
}
function isnot(a, b, description) {
  send({kind: "isnot", a: a, b:b, description:description});
}

function test_init() {
  ok(true, "Starting test_init");
  importScripts("resource:///modules/osfile.jsm");
}


/**
 * Test that we can open an existing file.
 */
function test_open_existing_file()
{
  ok(true, "Starting test_open_existing");
  let file = OS.File.open("chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js");
  file.close();
}

/**
 * Test that opening a file that does not exist fails with the right error.
 */
function test_open_non_existing_file()
{
  ok(true, "Starting test_open_non_existing");
  let exn;
  try {
    let file = OS.File.open("/I do not exist");
  } catch (x) {
    exn = x;
    ok(true, "test_open_non_existing_file: Exception detail " + exn);
  }
  ok(!!exn, "test_open_non_existing_file: Exception was raised ");
  ok(exn instanceof OS.File.Error, "test_open_non_existing_file: Exception was a OS.File.Error");
  ok(exn.becauseNoSuchFile, "test_open_non_existing_file: Exception confirms that the file does not exist");
}

/**
 * Utility function for comparing two files.
 */
function compare_files(test, sourcePath, destPath)
{
  ok(true, test + ": Comparing " + sourcePath + " and " + destPath);
  let source = OS.File.open(sourcePath);
  let dest = OS.File.open(destPath);
  ok(true, "Files are open");
  let array1 = new (ctypes.ArrayType(ctypes.char, 4096))();
  let array2 = new (ctypes.ArrayType(ctypes.char, 4096))();
  ok(true, "Arrays are created");
  let pos = 0;
  while (true) {
    ok(true, "Position: "+pos);
    let bytes_read1 = source.read(array1, 4096);
    let bytes_read2 = dest.read(array2, 4096);
    is (bytes_read1 > 0, bytes_read2 > 0,
       test + ": Both files contain data or neither does " +
        bytes_read1 + ", " + bytes_read2);
    if (bytes_read1 == 0) {
      break;
    }
    let bytes;
    if (bytes_read1 != bytes_read2) {
      // This would be surprising, but theoretically possible with a
      // remote file system, I believe.
      bytes = Math.min(bytes_read1, bytes_read2);
      pos += bytes;
      source.setPosition(pos, OS.File.POS_START);
      dest.setPosition(pos, OS.File.POS_START);
    } else {
      bytes = bytes_read1;
      pos += bytes;
    }
    for (let i = 0; i < bytes; ++i) {
      if (array1[i] != array2[i]) {
        ok(false, test + ": Files do not match at position " + i
           + " (" + array1[i] + "/ " + array2[i] + ")");
      }
    }
  }
  source.close();
  dest.close();
  ok(true, test + ": Comparison complete");
}

/**
 * Test basic read/write through an ArrayBuffer
 */
function test_read_write_file()
{
  let src_file_name = "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js";
  let tmp_file_name = "test_osfile_front.tmp";
  ok(true, "Starting test_read_write_file");

  let source = OS.File.open(src_file_name);
  let dest = OS.File.open(tmp_file_name, {write: true, trunc:true});

  let buf = new ArrayBuffer(4096);
  for (let bytesAvailable = source.read(buf, 4096);
         bytesAvailable != 0;
         bytesAvailable = source.read(buf, 4096)) {
    let bytesWritten = dest.write(buf, bytesAvailable);
    if (bytesWritten != bytesAvailable) {
      is(bytesWritten, bytesAvailable, "test_read_write_file: writing all bytes");
    }
  }

  ok(true, "test_read_write_file: copy complete");
  source.close();
  dest.close();

  compare_files("test_read_write_file", src_file_name, tmp_file_name);
  OS.File.remove(tmp_file_name);
}

/**
 * Test that copying a file using |copy| works.
 */
function test_copy_existing_file()
{
  let src_file_name = "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js";
  let tmp_file_name = "test_osfile_front.tmp";
  ok(true, "Starting test_copy_existing");
  OS.File.copy(src_file_name, tmp_file_name);

  ok(true, "test_copy_existing: Copy complete");
  compare_files("test_copy_existing", src_file_name, tmp_file_name);

  ok(true, "test_copy_existing: Cleaning up");
  OS.File.remove(tmp_file_name);
}

/**
 * Test that moving a file works.
 */
function test_move_file()
{
  ok(true, "test_move_file: Starting");
  // 1. Copy file into a temporary file
  let src_file_name = "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js";
  let tmp_file_name = "test_osfile_front.tmp";
  let tmp2_file_name = "test_osfile_front.tmp2";
  OS.File.copy(src_file_name, tmp_file_name);

  ok(true, "test_move_file: Copy complete");

  // 2. Move
  OS.File.move(tmp_file_name, tmp2_file_name);

  ok(true, "test_move_file: Move complete");

  // 3. Check
  compare_files("test_move_file", src_file_name, tmp2_file_name);

  ok(true, "test_move_file: Cleaning up");
  OS.File.remove(tmp2_file_name);
}


function test_iter_dir()
{
  ok(true, "test_iter_dir: Starting");

  // Create a file, to be sure that it exists
  let tmp_file_name = "test_osfile_front.tmp";
  let tmp_file = OS.File.open(tmp_file_name, {write: true, trunc:true});
  tmp_file.close();

  let parent = OS.File.curDir;
  ok(true, "test_iter_dir: directory " + parent);
  let iterator = new OS.File.DirectoryIterator(parent);
  ok(true, "test_iter_dir: iterator created");
  let encountered_tmp_file = false;
  for (let entry in iterator) {
    // Checking that |name| can be decoded properly
    ok(true, "test_iter_dir: encountering entry " + entry.name);

    if (entry.name == tmp_file_name) {
      encountered_tmp_file = true;
      isnot(entry.isDir, "test_iter_dir: The temporary file is not a directory");
      isnot(entry.isSymLink, "test_iter_dir: The temporary file is not a link");
    }

    let file;
    let success = true;
    try {
      file = OS.File.open(entry.path);
    } catch (x) {
      if (x.becauseNoSuchFile) {
        success = false;
      }
    }
    if (file) {
      file.close();
    }
    ok(success, "test_iter_dir: Entry " + entry.path + " exists");

    if (OS.Win) {
      let year = new Date().getFullYear();
      let creation = entry.winCreationTime;
      ok(creation, "test_iter_dir: Windows creation date exists: " + creation);
      ok(creation.getFullYear() >= year -  1 && creation.getFullYear() <= year, "test_iter_dir: consistent creation date");

      let lastWrite = entry.winLastWriteTime;
      ok(lastWrite, "test_iter_dir: Windows lastWrite date exists: " + lastWrite);
      ok(lastWrite.getFullYear() >= year - 1 && lastWrite.getFullYear() <= year, "test_iter_dir: consistent lastWrite date");

      let lastAccess = entry.winLastAccessTime;
      ok(lastAccess, "test_iter_dir: Windows lastAccess date exists: " + lastAccess);
      ok(lastAccess.getFullYear() >= year - 1 && lastAccess.getFullYear() <= year, "test_iter_dir: consistent lastAccess date");
    }

  }
  ok(encountered_tmp_file, "test_iter_dir: We have found the temporary file");

  ok(true, "test_iter_dir: Cleaning up");
  iterator.close();
  ok(true, "test_iter_dir: Complete");
}

function test_info() {
  ok(true, "test_info: Starting");

  let filename = "test_info.tmp";
  let size = 261;// An arbitrary file length
  let start = new Date();

 // Cleanup any leftover from previous tests
  try {
    OS.File.remove(filename);
    ok(true, "test_info: Cleaned up previous garbage");
  } catch (x) {
    if (!x.becauseNoSuchFile) {
      throw x;
    }
    ok(true, "test_info: No previous garbage");
  }

  let file = OS.File.open(filename, {trunc: true});
  let buf = new ArrayBuffer(size);
  file.write(buf, size);
  file.close();

  // Test OS.File.stat on new file
  let info = OS.File.stat(filename);
  ok(!!info, "test_info: info acquired");
  ok(!info.isDir, "test_info: file is not a directory");
  is(info.isSymLink, false, "test_info: file is not a link");
  is(info.size.toString(), size, "test_info: correct size");

  let stop = new Date();

  // We round down/up by 1s as file system precision is lower than Date precision
  let startMs = start.getTime() - 1000;
  let stopMs  = stop.getTime() + 1000;

  let birth = info.creationDate;
  ok(birth.getTime() <= stopMs,
     "test_info: file was created before now - " + stop + ", " + birth);
  // Note: Previous versions of this test checked whether the file has
  // been created after the start of the test. Unfortunately, this sometimes
  // failed under Windows, in specific circumstances: if the file has been
  // removed at the start of the test and recreated immediately, the Windows
  // file system detects this and decides that the file was actually truncated
  // rather than recreated, hence that it should keep its previous creation date.
  // Debugging hilarity ensues.

  let change = info.lastModificationDate;
  ok(change.getTime() >= startMs
     && change.getTime() <= stopMs,
     "test_info: file has changed between the start of the test and now - " + start + ", " + stop + ", " + change);

  // Test OS.File.prototype.stat on new file
  file = OS.File.open(filename);
  try {
    info = file.stat();
  } finally {
    file.close();
  }

  ok(!!info, "test_info: info acquired 2");
  ok(!info.isDir, "test_info: file is not a directory 2");
  ok(!info.isSymLink, "test_info: file is not a link 2");
  is(info.size.toString(), size, "test_info: correct size 2");

  stop = new Date();

  // We round down/up by 1s as file system precision is lower than Date precision
  startMs = start.getTime() - 1000;
  stopMs  = stop.getTime() + 1000;

  birth = info.creationDate;
  ok(birth.getTime() <= stopMs,
      "test_info: file 2 was created between the start of the test and now - " + start +  ", " + stop + ", " + birth);

  let access = info.lastModificationDate;
  ok(access.getTime() >= startMs
     && access.getTime() <= stopMs,
     "test_info: file 2 was accessed between the start of the test and now - " + start + ", " + stop + ", " + access);

  change = info.lastModificationDate;
  ok(change.getTime() >= startMs
     && change.getTime() <= stopMs,
     "test_info: file 2 has changed between the start of the test and now - " + start + ", " + stop + ", " + change);

  // Test OS.File.stat on directory
  info = OS.File.stat(OS.File.curDir);
  ok(!!info, "test_info: info on directory acquired");
  ok(info.isDir, "test_info: directory is a directory");

  ok(true, "test_info: Complete");
}
