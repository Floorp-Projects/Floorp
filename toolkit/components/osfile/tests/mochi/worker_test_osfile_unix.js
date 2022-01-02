/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log */

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");

self.onmessage = function(msg) {
  log("received message " + JSON.stringify(msg.data));
  self.onmessage = function(msg) {
    log("ignored message " + JSON.stringify(msg.data));
  };
  test_init();
  test_getcwd();
  test_open_close();
  test_create_file();
  test_access();
  test_read_write();
  test_passing_undefined();
  finish();
};

function test_init() {
  info("Starting test_init");
  importScripts("resource://gre/modules/osfile.jsm");
}

function test_open_close() {
  info("Starting test_open_close");
  is(typeof OS.Unix.File.open, "function", "OS.Unix.File.open is a function");
  let file = OS.Unix.File.open(
    "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js",
    OS.Constants.libc.O_RDONLY
  );
  isnot(file, -1, "test_open_close: opening succeeded");
  info("Close: " + OS.Unix.File.close.toSource());
  let result = OS.Unix.File.close(file);
  is(result, 0, "test_open_close: close succeeded");

  file = OS.Unix.File.open("/i do not exist", OS.Constants.libc.O_RDONLY);
  is(file, -1, "test_open_close: opening of non-existing file failed");
  is(
    ctypes.errno,
    OS.Constants.libc.ENOENT,
    "test_open_close: error is ENOENT"
  );
}

function test_create_file() {
  info("Starting test_create_file");
  let file = OS.Unix.File.open(
    "test.tmp",
    OS.Constants.libc.O_RDWR |
      OS.Constants.libc.O_CREAT |
      OS.Constants.libc.O_TRUNC,
    ctypes.int(OS.Constants.libc.S_IRWXU)
  );
  isnot(file, -1, "test_create_file: file created");
  OS.Unix.File.close(file);
}

function test_access() {
  info("Starting test_access");
  let file = OS.Unix.File.open(
    "test1.tmp",
    OS.Constants.libc.O_RDWR |
      OS.Constants.libc.O_CREAT |
      OS.Constants.libc.O_TRUNC,
    ctypes.int(OS.Constants.libc.S_IRWXU)
  );
  let result = OS.Unix.File.access(
    "test1.tmp",
    OS.Constants.libc.R_OK |
      OS.Constants.libc.W_OK |
      OS.Constants.libc.X_OK |
      OS.Constants.libc.F_OK
  );
  is(result, 0, "first call to access() succeeded");
  OS.Unix.File.close(file);

  file = OS.Unix.File.open(
    "test1.tmp",
    OS.Constants.libc.O_WRONLY |
      OS.Constants.libc.O_CREAT |
      OS.Constants.libc.O_TRUNC,
    ctypes.int(OS.Constants.libc.S_IWUSR)
  );

  info("test_access: preparing second call to access()");
  result = OS.Unix.File.access(
    "test2.tmp",
    OS.Constants.libc.R_OK |
      OS.Constants.libc.W_OK |
      OS.Constants.libc.X_OK |
      OS.Constants.libc.F_OK
  );
  is(result, -1, "test_access: second call to access() failed as expected");
  is(ctypes.errno, OS.Constants.libc.ENOENT, "This is the correct error");
  OS.Unix.File.close(file);
}

function test_getcwd() {
  let array = new (ctypes.ArrayType(ctypes.char, 32768))();
  let path = OS.Unix.File.getcwd(array, array.length);
  if (ctypes.char.ptr(path).isNull()) {
    ok(false, "test_get_cwd: getcwd returned null, errno: " + ctypes.errno);
  }
  let path2;
  if (OS.Unix.File.get_current_dir_name) {
    path2 = OS.Unix.File.get_current_dir_name();
  } else {
    path2 = OS.Unix.File.getwd_auto(null);
  }
  if (ctypes.char.ptr(path2).isNull()) {
    ok(
      false,
      "test_get_cwd: getwd_auto/get_current_dir_name returned null, errno: " +
        ctypes.errno
    );
  }
  is(
    path.readString(),
    path2.readString(),
    "test_get_cwd: getcwd and getwd return the same path"
  );
}

function test_read_write() {
  let output_name = "osfile_copy.tmp";
  // Copy file
  let input = OS.Unix.File.open(
    "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js",
    OS.Constants.libc.O_RDONLY
  );
  isnot(input, -1, "test_read_write: input file opened");
  let output = OS.Unix.File.open(
    "osfile_copy.tmp",
    OS.Constants.libc.O_RDWR |
      OS.Constants.libc.O_CREAT |
      OS.Constants.libc.O_TRUNC,
    ctypes.int(OS.Constants.libc.S_IRWXU)
  );
  isnot(output, -1, "test_read_write: output file opened");

  let array = new (ctypes.ArrayType(ctypes.char, 4096))();
  let bytes = -1;
  let total = 0;
  while (true) {
    bytes = OS.Unix.File.read(input, array, 4096);
    ok(bytes != undefined, "test_read_write: bytes is defined");
    isnot(bytes, -1, "test_read_write: no read error");
    let write_from = 0;
    if (bytes == 0) {
      break;
    }
    while (bytes > 0) {
      array.addressOfElement(write_from);
      // Note: |write| launches an exception in case of error
      let written = OS.Unix.File.write(output, array, bytes);
      isnot(written, -1, "test_read_write: no write error");
      write_from += written;
      bytes -= written;
    }
    total += write_from;
  }
  info("test_read_write: copy complete " + total);

  // Compare files
  let result;
  info("SEEK_SET: " + OS.Constants.libc.SEEK_SET);
  info("Input: " + input + "(" + input.toSource() + ")");
  info("Output: " + output + "(" + output.toSource() + ")");
  result = OS.Unix.File.lseek(input, 0, OS.Constants.libc.SEEK_SET);
  info("Result of lseek: " + result);
  isnot(result, -1, "test_read_write: input seek succeeded " + ctypes.errno);
  result = OS.Unix.File.lseek(output, 0, OS.Constants.libc.SEEK_SET);
  isnot(result, -1, "test_read_write: output seek succeeded " + ctypes.errno);

  let array2 = new (ctypes.ArrayType(ctypes.char, 4096))();
  let bytes2 = -1;
  let pos = 0;
  while (true) {
    bytes = OS.Unix.File.read(input, array, 4096);
    isnot(bytes, -1, "test_read_write: input read succeeded");
    bytes2 = OS.Unix.File.read(output, array2, 4096);
    isnot(bytes, -1, "test_read_write: output read succeeded");
    is(
      bytes > 0,
      bytes2 > 0,
      "Both files contain data or neither does " + bytes + ", " + bytes2
    );
    if (bytes == 0) {
      break;
    }
    if (bytes != bytes2) {
      // This would be surprising, but theoretically possible with a
      // remote file system, I believe.
      bytes = Math.min(bytes, bytes2);
      pos += bytes;
      result = OS.Unix.File.lseek(input, pos, OS.Constants.libc.SEEK_SET);
      isnot(result, -1, "test_read_write: input seek succeeded");
      result = OS.Unix.File.lseek(output, pos, OS.Constants.libc.SEEK_SET);
      isnot(result, -1, "test_read_write: output seek succeeded");
    } else {
      pos += bytes;
    }
    for (let i = 0; i < bytes; ++i) {
      if (array[i] != array2[i]) {
        ok(
          false,
          "Files do not match at position " +
            i +
            " (" +
            array[i] +
            "/" +
            array2[i] +
            ")"
        );
      }
    }
  }
  info("test_read_write test complete");
  result = OS.Unix.File.close(input);
  isnot(result, -1, "test_read_write: input close succeeded");
  result = OS.Unix.File.close(output);
  isnot(result, -1, "test_read_write: output close succeeded");
  result = OS.Unix.File.unlink(output_name);
  isnot(result, -1, "test_read_write: input remove succeeded");
  info("test_read_write cleanup complete");
}

function test_passing_undefined() {
  info(
    "Testing that an exception gets thrown when an FFI function is passed undefined"
  );
  let exceptionRaised = false;

  try {
    OS.Unix.File.open(
      undefined,
      OS.Constants.libc.O_RDWR |
        OS.Constants.libc.O_CREAT |
        OS.Constants.libc.O_TRUNC,
      ctypes.int(OS.Constants.libc.S_IRWXU)
    );
  } catch (e) {
    if (e instanceof TypeError && e.message.indexOf("open") > -1) {
      exceptionRaised = true;
    } else {
      throw e;
    }
  }

  ok(exceptionRaised, "test_passing_undefined: exception gets thrown");
}
