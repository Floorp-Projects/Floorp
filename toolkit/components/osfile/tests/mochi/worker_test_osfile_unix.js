/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function log(text) {
  dump("WORKER "+text+"\n");
}

function send(message) {
  self.postMessage(message);
}

self.onmessage = function(msg) {
  log("received message "+JSON.stringify(msg.data));
  self.onmessage = function(msg) {
    log("ignored message "+JSON.stringify(msg.data));
  };
  test_init();
  test_getcwd();
  test_open_close();
  test_create_file();
  test_access();
  test_read_write();
  test_path();
  finish();
};

function finish() {
  send({kind: "finish"});
}

function ok(condition, description) {
  send({kind: "ok", condition: condition, description:description});
}
function is(a, b, description) {
  let outcome = a == b; // Need to decide outcome here, as not everything can be serialized
  send({kind: "is", outcome: outcome, description: description, a:""+a, b:""+b});
}
function isnot(a, b, description) {
  let outcome = a != b; // Need to decide outcome here, as not everything can be serialized
  send({kind: "isnot", outcome: outcome, description: description, a:""+a, b:""+b});
}

function test_init() {
  ok(true, "Starting test_init");
  importScripts("resource:///modules/osfile.jsm");
}

function test_open_close() {
  ok(true, "Starting test_open_close");
  is(typeof OS.Unix.File.open, "function", "OS.Unix.File.open is a function");
  let file = OS.Unix.File.open("chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js", OS.Constants.libc.O_RDONLY, 0);
  isnot(file, -1, "test_open_close: opening succeeded");
  ok(true, "Close: "+OS.Unix.File.close.toSource());
  let result = OS.Unix.File.close(file);
  is(result, 0, "test_open_close: close succeeded");

  file = OS.Unix.File.open("/i do not exist", OS.Constants.libc.O_RDONLY, 0);
  is(file, -1, "test_open_close: opening of non-existing file failed");
  is(ctypes.errno, OS.Constants.libc.ENOENT, "test_open_close: error is ENOENT");
}

function test_create_file()
{
  ok(true, "Starting test_create_file");
  let file = OS.Unix.File.open("test.tmp", OS.Constants.libc.O_RDWR
                               | OS.Constants.libc.O_CREAT
                               | OS.Constants.libc.O_TRUNC,
                               OS.Constants.libc.S_IRWXU);
  isnot(file, -1, "test_create_file: file created");
  OS.Unix.File.close(file);
}

function test_access()
{
  ok(true, "Starting test_access");
  let file = OS.Unix.File.open("test1.tmp", OS.Constants.libc.O_RDWR
                               | OS.Constants.libc.O_CREAT
                               | OS.Constants.libc.O_TRUNC,
                               OS.Constants.libc.S_IRWXU);
  let result = OS.Unix.File.access("test1.tmp", OS.Constants.libc.R_OK | OS.Constants.libc.W_OK | OS.Constants.libc.X_OK | OS.Constants.libc.F_OK);
  is(result, 0, "first call to access() succeeded");
  OS.Unix.File.close(file);

  file = OS.Unix.File.open("test1.tmp", OS.Constants.libc.O_WRONLY
                               | OS.Constants.libc.O_CREAT
                               | OS.Constants.libc.O_TRUNC,
                               OS.Constants.libc.S_IWUSR);

  ok(true, "test_access: preparing second call to access()");
  result = OS.Unix.File.access("test2.tmp", OS.Constants.libc.R_OK
                        | OS.Constants.libc.W_OK
                        | OS.Constants.libc.X_OK
                        | OS.Constants.libc.F_OK);
  is(result, -1, "test_access: second call to access() failed as expected");
  is(ctypes.errno, OS.Constants.libc.ENOENT, "This is the correct error");
  OS.Unix.File.close(file);
}

function test_getcwd()
{
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
    ok(false, "test_get_cwd: getwd_auto/get_current_dir_name returned null, errno: " + ctypes.errno);
  }
  is(path.readString(), path2.readString(), "test_get_cwd: getcwd and getwd return the same path");
}

function test_read_write()
{
  let output_name = "osfile_copy.tmp";
  // Copy file
  let input = OS.Unix.File.open(
    "chrome/toolkit/components/osfile/tests/mochi/worker_test_osfile_unix.js",
    OS.Constants.libc.O_RDONLY, 0);
  isnot(input, -1, "test_read_write: input file opened");
  let output = OS.Unix.File.open("osfile_copy.tmp", OS.Constants.libc.O_RDWR
    | OS.Constants.libc.O_CREAT
    | OS.Constants.libc.O_TRUNC,
    OS.Constants.libc.S_IRWXU);
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
      let ptr = array.addressOfElement(write_from);
      // Note: |write| launches an exception in case of error
      let written = OS.Unix.File.write(output, array, bytes);
      isnot(written, -1, "test_read_write: no write error");
      write_from += written;
      bytes -= written;
    }
    total += write_from;
  }
  ok(true, "test_read_write: copy complete " + total);

  // Compare files
  let result;
  ok(true, "SEEK_SET: " + OS.Constants.libc.SEEK_SET);
  ok(true, "Input: " + input + "(" + input.toSource() + ")");
  ok(true, "Output: " + output + "(" + output.toSource() + ")");
  result = OS.Unix.File.lseek(input, 0, OS.Constants.libc.SEEK_SET);
  ok(true, "Result of lseek: " + result);
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
    is(bytes > 0, bytes2 > 0, "Both files contain data or neither does "+bytes+", "+bytes2);
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
        ok(false, "Files do not match at position " + i
           + " ("+array[i] + "/"+array2[i] + ")");
      }
    }
  }
  ok(true, "test_read_write test complete");
  result = OS.Unix.File.close(input);
  isnot(result, -1, "test_read_write: input close succeeded");
  result = OS.Unix.File.close(output);
  isnot(result, -1, "test_read_write: output close succeeded");
  result = OS.Unix.File.unlink(output_name);
  isnot(result, -1, "test_read_write: input remove succeeded");
  ok(true, "test_read_write cleanup complete");
}

function test_path()
{
  ok(true, "test_path: starting");
  is(OS.Unix.Path.basename("a/b"), "b", "basename of a/b");
  is(OS.Unix.Path.basename("a/b/"), "", "basename of a/b/");
  is(OS.Unix.Path.basename("abc"), "abc", "basename of abc");
  is(OS.Unix.Path.dirname("a/b"), "a", "basename of a/b");
  is(OS.Unix.Path.dirname("a/b/"), "a/b", "basename of a/b/");
  is(OS.Unix.Path.dirname("a////b"), "a", "basename of a///b");
  is(OS.Unix.Path.dirname("abc"), ".", "basename of abc");
  is(OS.Unix.Path.normalize("/a/b/c"), "/a/b/c", "normalize /a/b/c");
  is(OS.Unix.Path.normalize("/a/b////c"), "/a/b/c", "normalize /a/b////c");
  is(OS.Unix.Path.normalize("////a/b/c"), "/a/b/c", "normalize ///a/b/c");
  is(OS.Unix.Path.normalize("/a/b/c///"), "/a/b/c", "normalize /a/b/c///");
  is(OS.Unix.Path.normalize("/a/b/c/../../../d/e/f"), "/d/e/f", "normalize /a/b/c/../../../d/e/f");
  is(OS.Unix.Path.normalize("a/b/c/../../../d/e/f"), "d/e/f", "normalize a/b/c/../../../d/e/f");
  let error = false;
  try {
    OS.Unix.Path.normalize("/a/b/c/../../../../d/e/f");
  } catch (x) {
    error = true;
  }
  ok(error, "cannot normalize /a/b/c/../../../../d/e/f");
  is(OS.Unix.Path.join("/tmp", "foo", "bar"), "/tmp/foo/bar", "join /tmp,foo,bar");
  is(OS.Unix.Path.join("/tmp", "/foo", "bar"), "/foo/bar", "join /tmp,/foo,bar");
  ok(true, "test_path: complete");
}