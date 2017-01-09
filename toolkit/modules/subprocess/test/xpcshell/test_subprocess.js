"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");


const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

const MAX_ROUND_TRIP_TIME_MS = AppConstants.DEBUG || AppConstants.ASAN ? 18 : 9;
const MAX_RETRIES = 5;

let PYTHON;
let PYTHON_BIN;
let PYTHON_DIR;

const TEST_SCRIPT = do_get_file("data_test_script.py").path;

let read = pipe => {
  return pipe.readUint32().then(count => {
    return pipe.readString(count);
  });
};


let readAll = Task.async(function* (pipe) {
  let result = [];
  let string;
  while ((string = yield pipe.readString())) {
    result.push(string);
  }

  return result.join("");
});


add_task(function* setup() {
  PYTHON = yield Subprocess.pathSearch(env.get("PYTHON"));

  PYTHON_BIN = OS.Path.basename(PYTHON);
  PYTHON_DIR = OS.Path.dirname(PYTHON);
});


add_task(function* test_subprocess_io() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  Assert.throws(() => { proc.stdout.read(-1); },
                /non-negative integer/);
  Assert.throws(() => { proc.stdout.read(1.1); },
                /non-negative integer/);

  Assert.throws(() => { proc.stdout.read(Infinity); },
                /non-negative integer/);
  Assert.throws(() => { proc.stdout.read(NaN); },
                /non-negative integer/);

  Assert.throws(() => { proc.stdout.readString(-1); },
                /non-negative integer/);
  Assert.throws(() => { proc.stdout.readString(1.1); },
                /non-negative integer/);

  Assert.throws(() => { proc.stdout.readJSON(-1); },
                /positive integer/);
  Assert.throws(() => { proc.stdout.readJSON(0); },
                /positive integer/);
  Assert.throws(() => { proc.stdout.readJSON(1.1); },
                /positive integer/);


  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";


  let outputPromise = read(proc.stdout);

  yield new Promise(resolve => setTimeout(resolve, 100));

  let [output] = yield Promise.all([
    outputPromise,
    proc.stdin.write(LINE1),
  ]);

  equal(output, LINE1, "Got expected output");


  // Make sure it succeeds whether the write comes before or after the
  // read.
  let inputPromise = proc.stdin.write(LINE2);

  yield new Promise(resolve => setTimeout(resolve, 100));

  [output] = yield Promise.all([
    read(proc.stdout),
    inputPromise,
  ]);

  equal(output, LINE2, "Got expected output");


  let JSON_BLOB = {foo: {bar: "baz"}};

  inputPromise = proc.stdin.write(JSON.stringify(JSON_BLOB) + "\n");

  output = yield proc.stdout.readUint32().then(count => {
    return proc.stdout.readJSON(count);
  });

  Assert.deepEqual(output, JSON_BLOB, "Got expected JSON output");


  yield proc.stdin.close();

  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_large_io() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE = "I'm a leaf on the wind.\n";
  const BUFFER_SIZE = 4096;

  // Create a message that's ~3/4 the input buffer size.
  let msg = Array(BUFFER_SIZE * .75 / 16 | 0).fill("0123456789abcdef").join("") + "\n";

  // This sequence of writes and reads crosses several buffer size
  // boundaries, and causes some branches of the read buffer code to be
  // exercised which are not exercised by other tests.
  proc.stdin.write(msg);
  proc.stdin.write(msg);
  proc.stdin.write(LINE);

  let output = yield read(proc.stdout);
  equal(output, msg, "Got the expected output");

  output = yield read(proc.stdout);
  equal(output, msg, "Got the expected output");

  output = yield read(proc.stdout);
  equal(output, LINE, "Got the expected output");

  proc.stdin.close();


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_huge() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // This should be large enough to fill most pipe input/output buffers.
  const MESSAGE_SIZE = 1024 * 16;

  let msg = Array(MESSAGE_SIZE).fill("0123456789abcdef").join("") + "\n";

  proc.stdin.write(msg);

  let output = yield read(proc.stdout);
  equal(output, msg, "Got the expected output");

  proc.stdin.close();


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_round_trip_perf() {
  let roundTripTime = Infinity;
  for (let i = 0; i < MAX_RETRIES && roundTripTime > MAX_ROUND_TRIP_TIME_MS; i++) {
    let proc = yield Subprocess.call({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "echo"],
    });


    const LINE = "I'm a leaf on the wind.\n";

    let now = Date.now();
    const COUNT = 1000;
    for (let j = 0; j < COUNT; j++) {
      let [output] = yield Promise.all([
        read(proc.stdout),
        proc.stdin.write(LINE),
      ]);

      // We don't want to log this for every iteration, but we still need
      // to fail if it goes wrong.
      if (output !== LINE) {
        equal(output, LINE, "Got expected output");
      }
    }

    roundTripTime = (Date.now() - now) / COUNT;

    yield proc.stdin.close();

    let {exitCode} = yield proc.wait();

    equal(exitCode, 0, "Got expected exit code");
  }

  ok(roundTripTime <= MAX_ROUND_TRIP_TIME_MS,
     `Expected round trip time (${roundTripTime}ms) to be less than ${MAX_ROUND_TRIP_TIME_MS}ms`);
});


add_task(function* test_subprocess_stderr_default() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
  });

  equal(proc.stderr, undefined, "There should be no stderr pipe by default");

  let stdout = yield readAll(proc.stdout);

  equal(stdout, LINE1, "Got the expected stdout output");


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_stderr_pipe() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "pipe",
  });

  let [stdout, stderr] = yield Promise.all([
    readAll(proc.stdout),
    readAll(proc.stderr),
  ]);

  equal(stdout, LINE1, "Got the expected stdout output");
  equal(stderr, LINE2, "Got the expected stderr output");


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_stderr_merged() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "stdout",
  });

  equal(proc.stderr, undefined, "There should be no stderr pipe by default");

  let stdout = yield readAll(proc.stdout);

  equal(stdout, LINE1 + LINE2, "Got the expected merged stdout output");


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_read_after_exit() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "pipe",
  });


  let {exitCode} = yield proc.wait();
  equal(exitCode, 0, "Process exited with expected code");


  let [stdout, stderr] = yield Promise.all([
    readAll(proc.stdout),
    readAll(proc.stderr),
  ]);

  equal(stdout, LINE1, "Got the expected stdout output");
  equal(stderr, LINE2, "Got the expected stderr output");
});


add_task(function* test_subprocess_lazy_close_output() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let writePromises = [
    proc.stdin.write(LINE1),
    proc.stdin.write(LINE2),
  ];
  let closedPromise = proc.stdin.close();


  let output1 = yield read(proc.stdout);
  let output2 = yield read(proc.stdout);

  yield Promise.all([...writePromises, closedPromise]);

  equal(output1, LINE1, "Got expected output");
  equal(output2, LINE2, "Got expected output");


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_lazy_close_input() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();
  let closedPromise = proc.stdout.close();


  const LINE = "I'm a leaf on the wind.\n";

  proc.stdin.write(LINE);
  proc.stdin.close();

  let len = yield readPromise;
  equal(len, LINE.length);

  yield closedPromise;


  // Don't test for a successful exit here. The process may exit with a
  // write error if we close the pipe after it's written the message
  // size but before it's written the message.
  yield proc.wait();
});


add_task(function* test_subprocess_force_close() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();
  let closedPromise = proc.stdout.close(true);

  yield Assert.rejects(
    readPromise,
    function(e) {
      equal(e.errorCode, Subprocess.ERROR_END_OF_FILE,
            "Got the expected error code");
      return /File closed/.test(e.message);
    },
    "Promise should be rejected when file is closed");

  yield closedPromise;
  yield proc.stdin.close();


  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_eof() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();

  yield proc.stdin.close();

  yield Assert.rejects(
    readPromise,
    function(e) {
      equal(e.errorCode, Subprocess.ERROR_END_OF_FILE,
            "Got the expected error code");
      return /File closed/.test(e.message);
    },
    "Promise should be rejected on EOF");

  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_subprocess_invalid_json() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE = "I'm a leaf on the wind.\n";

  proc.stdin.write(LINE);
  proc.stdin.close();

  let count = yield proc.stdout.readUint32();
  let readPromise = proc.stdout.readJSON(count);

  yield Assert.rejects(
    readPromise,
    function(e) {
      equal(e.errorCode, Subprocess.ERROR_INVALID_JSON,
            "Got the expected error code");
      return /SyntaxError/.test(e);
    },
    "Promise should be rejected on EOF");

  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


if (AppConstants.isPlatformAndVersionAtLeast("win", "6")) {
  add_task(function* test_subprocess_inherited_descriptors() {
    let {ctypes, libc, win32} = Cu.import("resource://gre/modules/subprocess/subprocess_win.jsm", {});

    let secAttr = new win32.SECURITY_ATTRIBUTES();
    secAttr.nLength = win32.SECURITY_ATTRIBUTES.size;
    secAttr.bInheritHandle = true;

    let handles = win32.createPipe(secAttr, 0);


    let proc = yield Subprocess.call({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "echo"],
    });


    // Close the output end of the pipe.
    // Ours should be the only copy, so reads should fail after this.
    handles[1].dispose();

    let buffer = new ArrayBuffer(1);
    let succeeded = libc.ReadFile(handles[0], buffer, buffer.byteLength,
                                  null, null);

    ok(!succeeded, "ReadFile should fail on broken pipe");
    equal(ctypes.winLastError, win32.ERROR_BROKEN_PIPE, "Read should fail with ERROR_BROKEN_PIPE");


    proc.stdin.close();

    let {exitCode} = yield proc.wait();

    equal(exitCode, 0, "Got expected exit code");
  });
}


add_task(function* test_subprocess_wait() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "exit", "42"],
  });

  let {exitCode} = yield proc.wait();

  equal(exitCode, 42, "Got expected exit code");
});


add_task(function* test_subprocess_pathSearch() {
  let promise = Subprocess.call({
    command: PYTHON_BIN,
    arguments: ["-u", TEST_SCRIPT, "exit", "13"],
    environment: {
      PATH: PYTHON_DIR,
    },
  });

  yield Assert.rejects(
    promise,
    function(error) {
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable");
});


add_task(function* test_subprocess_workdir() {
  let procDir = yield OS.File.getCurrentDirectory();
  let tmpDir = OS.Constants.Path.tmpDir;

  notEqual(procDir, tmpDir,
           "Current process directory must not be the current temp directory");

  function* pwd(options) {
    let proc = yield Subprocess.call(Object.assign({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "pwd"],
    }, options));

    let pwdOutput = read(proc.stdout);

    let {exitCode} = yield proc.wait();
    equal(exitCode, 0, "Got expected exit code");

    return pwdOutput;
  }

  let dir = yield pwd({});
  equal(dir, procDir, "Process should normally launch in current process directory");

  dir = yield pwd({workdir: tmpDir});
  equal(dir, tmpDir, "Process should launch in the directory specified in `workdir`");

  dir = yield OS.File.getCurrentDirectory();
  equal(dir, procDir, "`workdir` should not change the working directory of the current process");
});


add_task(function* test_subprocess_term() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // Windows does not support killing processes gracefully, so they will
  // always exit with -9 there.
  let retVal = AppConstants.platform == "win" ? -9 : -15;

  // Kill gracefully with the default timeout of 300ms.
  let {exitCode} = yield proc.kill();

  equal(exitCode, retVal, "Got expected exit code");

  ({exitCode} = yield proc.wait());

  equal(exitCode, retVal, "Got expected exit code");
});


add_task(function* test_subprocess_kill() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // Force kill with no gracefull termination timeout.
  let {exitCode} = yield proc.kill(0);

  equal(exitCode, -9, "Got expected exit code");

  ({exitCode} = yield proc.wait());

  equal(exitCode, -9, "Got expected exit code");
});


add_task(function* test_subprocess_kill_timeout() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "ignore_sigterm"],
  });

  // Wait for the process to set up its signal handler and tell us it's
  // ready.
  let msg = yield read(proc.stdout);
  equal(msg, "Ready", "Process is ready");

  // Kill gracefully with the default timeout of 300ms.
  // Expect a force kill after 300ms, since the process traps SIGTERM.
  const TIMEOUT = 300;
  let startTime = Date.now();

  let {exitCode} = yield proc.kill(TIMEOUT);

  // Graceful termination is not supported on Windows, so don't bother
  // testing the timeout there.
  if (AppConstants.platform != "win") {
    let diff = Date.now() - startTime;
    ok(diff >= TIMEOUT, `Process was killed after ${diff}ms (expected ~${TIMEOUT}ms)`);
  }

  equal(exitCode, -9, "Got expected exit code");

  ({exitCode} = yield proc.wait());

  equal(exitCode, -9, "Got expected exit code");
});


add_task(function* test_subprocess_arguments() {
  let args = [
    String.raw`C:\Program Files\Company\Program.exe`,
    String.raw`\\NETWORK SHARE\Foo Directory${"\\"}`,
    String.raw`foo bar baz`,
    String.raw`"foo bar baz"`,
    String.raw`foo " bar`,
    String.raw`Thing \" with "" "\" \\\" \\\\" quotes\\" \\`,
  ];

  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print_args", ...args],
  });

  for (let [i, arg] of args.entries()) {
    let val = yield read(proc.stdout);
    equal(val, arg, `Got correct value for args[${i}]`);
  }

  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});


// Windows XP can't handle launching Python with a partial environment.
if (!AppConstants.isPlatformAndVersionAtMost("win", "5.2")) {
  add_task(function* test_subprocess_environment() {
    let proc = yield Subprocess.call({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "env", "PATH", "FOO"],
      environment: {
        FOO: "BAR",
      },
    });

    let path = yield read(proc.stdout);
    let foo = yield read(proc.stdout);

    equal(path, "", "Got expected $PATH value");
    equal(foo, "BAR", "Got expected $FOO value");

    let {exitCode} = yield proc.wait();

    equal(exitCode, 0, "Got expected exit code");
  });
}


add_task(function* test_subprocess_environmentAppend() {
  let proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "env", "PATH", "FOO"],
    environmentAppend: true,
    environment: {
      FOO: "BAR",
    },
  });

  let path = yield read(proc.stdout);
  let foo = yield read(proc.stdout);

  equal(path, env.get("PATH"), "Got expected $PATH value");
  equal(foo, "BAR", "Got expected $FOO value");

  let {exitCode} = yield proc.wait();

  equal(exitCode, 0, "Got expected exit code");

  proc = yield Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "env", "PATH", "FOO"],
    environmentAppend: true,
  });

  path = yield read(proc.stdout);
  foo = yield read(proc.stdout);

  equal(path, env.get("PATH"), "Got expected $PATH value");
  equal(foo, "", "Got expected $FOO value");

  ({exitCode} = yield proc.wait());

  equal(exitCode, 0, "Got expected exit code");
});


add_task(function* test_bad_executable() {
  // Test with a non-executable file.

  let textFile = do_get_file("data_text_file.txt").path;

  let promise = Subprocess.call({
    command: textFile,
    arguments: [],
  });

  yield Assert.rejects(
    promise,
    function(error) {
      if (AppConstants.platform == "win") {
        return /Failed to create process/.test(error.message);
      }
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable");

  // Test with a nonexistent file.
  promise = Subprocess.call({
    command: textFile + ".doesNotExist",
    arguments: [],
  });

  yield Assert.rejects(
    promise,
    function(error) {
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable");
});


add_task(function* test_cleanup() {
  let {SubprocessImpl} = Cu.import("resource://gre/modules/Subprocess.jsm", {});

  let worker = SubprocessImpl.Process.getWorker();

  let openFiles = yield worker.call("getOpenFiles", []);
  let processes = yield worker.call("getProcesses", []);

  equal(openFiles.size, 0, "No remaining open files");
  equal(processes.size, 0, "No remaining processes");
});
