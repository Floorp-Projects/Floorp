/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

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

let readAll = async function(pipe) {
  let result = [];
  let string;
  while ((string = await pipe.readString())) {
    result.push(string);
  }

  return result.join("");
};

add_task(async function setup() {
  PYTHON = await Subprocess.pathSearch(env.get("PYTHON"));

  PYTHON_BIN = OS.Path.basename(PYTHON);
  PYTHON_DIR = OS.Path.dirname(PYTHON);
});

add_task(async function test_subprocess_io() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  Assert.throws(() => {
    proc.stdout.read(-1);
  }, /non-negative integer/);
  Assert.throws(() => {
    proc.stdout.read(1.1);
  }, /non-negative integer/);

  Assert.throws(() => {
    proc.stdout.read(Infinity);
  }, /non-negative integer/);
  Assert.throws(() => {
    proc.stdout.read(NaN);
  }, /non-negative integer/);

  Assert.throws(() => {
    proc.stdout.readString(-1);
  }, /non-negative integer/);
  Assert.throws(() => {
    proc.stdout.readString(1.1);
  }, /non-negative integer/);

  Assert.throws(() => {
    proc.stdout.readJSON(-1);
  }, /positive integer/);
  Assert.throws(() => {
    proc.stdout.readJSON(0);
  }, /positive integer/);
  Assert.throws(() => {
    proc.stdout.readJSON(1.1);
  }, /positive integer/);

  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let outputPromise = read(proc.stdout);

  await new Promise(resolve => setTimeout(resolve, 100));

  let [output] = await Promise.all([outputPromise, proc.stdin.write(LINE1)]);

  equal(output, LINE1, "Got expected output");

  // Make sure it succeeds whether the write comes before or after the
  // read.
  let inputPromise = proc.stdin.write(LINE2);

  await new Promise(resolve => setTimeout(resolve, 100));

  [output] = await Promise.all([read(proc.stdout), inputPromise]);

  equal(output, LINE2, "Got expected output");

  let JSON_BLOB = { foo: { bar: "baz" } };

  inputPromise = proc.stdin.write(JSON.stringify(JSON_BLOB) + "\n");

  output = await proc.stdout.readUint32().then(count => {
    return proc.stdout.readJSON(count);
  });

  Assert.deepEqual(output, JSON_BLOB, "Got expected JSON output");

  await proc.stdin.close();

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_large_io() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE = "I'm a leaf on the wind.\n";
  const BUFFER_SIZE = 4096;

  // Create a message that's ~3/4 the input buffer size.
  let msg =
    Array(((BUFFER_SIZE * 0.75) / 16) | 0)
      .fill("0123456789abcdef")
      .join("") + "\n";

  // This sequence of writes and reads crosses several buffer size
  // boundaries, and causes some branches of the read buffer code to be
  // exercised which are not exercised by other tests.
  proc.stdin.write(msg);
  proc.stdin.write(msg);
  proc.stdin.write(LINE);

  let output = await read(proc.stdout);
  equal(output, msg, "Got the expected output");

  output = await read(proc.stdout);
  equal(output, msg, "Got the expected output");

  output = await read(proc.stdout);
  equal(output, LINE, "Got the expected output");

  proc.stdin.close();

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_huge() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // This should be large enough to fill most pipe input/output buffers.
  const MESSAGE_SIZE = 1024 * 16;

  let msg =
    Array(MESSAGE_SIZE)
      .fill("0123456789abcdef")
      .join("") + "\n";

  proc.stdin.write(msg);

  let output = await read(proc.stdout);
  equal(output, msg, "Got the expected output");

  proc.stdin.close();

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(
  { skip_if: () => mozinfo.ccov },
  async function test_subprocess_round_trip_perf() {
    let roundTripTime = Infinity;
    for (
      let i = 0;
      i < MAX_RETRIES && roundTripTime > MAX_ROUND_TRIP_TIME_MS;
      i++
    ) {
      let proc = await Subprocess.call({
        command: PYTHON,
        arguments: ["-u", TEST_SCRIPT, "echo"],
      });

      const LINE = "I'm a leaf on the wind.\n";

      let now = Date.now();
      const COUNT = 1000;
      for (let j = 0; j < COUNT; j++) {
        let [output] = await Promise.all([
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

      await proc.stdin.close();

      let { exitCode } = await proc.wait();

      equal(exitCode, 0, "Got expected exit code");
    }

    ok(
      roundTripTime <= MAX_ROUND_TRIP_TIME_MS,
      `Expected round trip time (${roundTripTime}ms) to be less than ${MAX_ROUND_TRIP_TIME_MS}ms`
    );
  }
);

add_task(async function test_subprocess_stderr_default() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
  });

  equal(proc.stderr, undefined, "There should be no stderr pipe by default");

  let stdout = await readAll(proc.stdout);

  equal(stdout, LINE1, "Got the expected stdout output");

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_stderr_pipe() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "pipe",
  });

  let [stdout, stderr] = await Promise.all([
    readAll(proc.stdout),
    readAll(proc.stderr),
  ]);

  equal(stdout, LINE1, "Got the expected stdout output");
  equal(stderr, LINE2, "Got the expected stderr output");

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_stderr_merged() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "stdout",
  });

  equal(proc.stderr, undefined, "There should be no stderr pipe by default");

  let stdout = await readAll(proc.stdout);

  equal(stdout, LINE1 + LINE2, "Got the expected merged stdout output");

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_read_after_exit() {
  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print", LINE1, LINE2],
    stderr: "pipe",
  });

  let { exitCode } = await proc.wait();
  equal(exitCode, 0, "Process exited with expected code");

  let [stdout, stderr] = await Promise.all([
    readAll(proc.stdout),
    readAll(proc.stderr),
  ]);

  equal(stdout, LINE1, "Got the expected stdout output");
  equal(stderr, LINE2, "Got the expected stderr output");
});

add_task(async function test_subprocess_lazy_close_output() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE1 = "I'm a leaf on the wind.\n";
  const LINE2 = "Watch how I soar.\n";

  let writePromises = [proc.stdin.write(LINE1), proc.stdin.write(LINE2)];
  let closedPromise = proc.stdin.close();

  let output1 = await read(proc.stdout);
  let output2 = await read(proc.stdout);

  await Promise.all([...writePromises, closedPromise]);

  equal(output1, LINE1, "Got expected output");
  equal(output2, LINE2, "Got expected output");

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_lazy_close_input() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();
  let closedPromise = proc.stdout.close();

  const LINE = "I'm a leaf on the wind.\n";

  proc.stdin.write(LINE);
  proc.stdin.close();

  let len = await readPromise;
  equal(len, LINE.length);

  await closedPromise;

  // Don't test for a successful exit here. The process may exit with a
  // write error if we close the pipe after it's written the message
  // size but before it's written the message.
  await proc.wait();
});

add_task(async function test_subprocess_force_close() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();
  let closedPromise = proc.stdout.close(true);

  await Assert.rejects(
    readPromise,
    function(e) {
      equal(
        e.errorCode,
        Subprocess.ERROR_END_OF_FILE,
        "Got the expected error code"
      );
      return /File closed/.test(e.message);
    },
    "Promise should be rejected when file is closed"
  );

  await closedPromise;
  await proc.stdin.close();

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_eof() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  let readPromise = proc.stdout.readUint32();

  await proc.stdin.close();

  await Assert.rejects(
    readPromise,
    function(e) {
      equal(
        e.errorCode,
        Subprocess.ERROR_END_OF_FILE,
        "Got the expected error code"
      );
      return /File closed/.test(e.message);
    },
    "Promise should be rejected on EOF"
  );

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_invalid_json() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  const LINE = "I'm a leaf on the wind.\n";

  proc.stdin.write(LINE);
  proc.stdin.close();

  let count = await proc.stdout.readUint32();
  let readPromise = proc.stdout.readJSON(count);

  await Assert.rejects(
    readPromise,
    function(e) {
      equal(
        e.errorCode,
        Subprocess.ERROR_INVALID_JSON,
        "Got the expected error code"
      );
      return /SyntaxError/.test(e);
    },
    "Promise should be rejected on EOF"
  );

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

if (AppConstants.isPlatformAndVersionAtLeast("win", "6")) {
  add_task(async function test_subprocess_inherited_descriptors() {
    let { libc, win32 } = ChromeUtils.import(
      "resource://gre/modules/subprocess/subprocess_win.jsm",
      null
    );
    const { ctypes } = ChromeUtils.import("resource://gre/modules/ctypes.jsm");

    let secAttr = new win32.SECURITY_ATTRIBUTES();
    secAttr.nLength = win32.SECURITY_ATTRIBUTES.size;
    secAttr.bInheritHandle = true;

    let handles = win32.createPipe(secAttr, 0);

    let proc = await Subprocess.call({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "echo"],
    });

    // Close the output end of the pipe.
    // Ours should be the only copy, so reads should fail after this.
    handles[1].dispose();

    let buffer = new ArrayBuffer(1);
    let succeeded = libc.ReadFile(
      handles[0],
      buffer,
      buffer.byteLength,
      null,
      null
    );

    ok(!succeeded, "ReadFile should fail on broken pipe");
    equal(
      ctypes.winLastError,
      win32.ERROR_BROKEN_PIPE,
      "Read should fail with ERROR_BROKEN_PIPE"
    );

    proc.stdin.close();

    let { exitCode } = await proc.wait();

    equal(exitCode, 0, "Got expected exit code");
  });
}

add_task(async function test_subprocess_wait() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "exit", "42"],
  });

  let { exitCode } = await proc.wait();

  equal(exitCode, 42, "Got expected exit code");
});

add_task(async function test_subprocess_pathSearch() {
  let promise = Subprocess.call({
    command: PYTHON_BIN,
    arguments: ["-u", TEST_SCRIPT, "exit", "13"],
    environment: {
      PATH: PYTHON_DIR,
    },
  });

  await Assert.rejects(
    promise,
    function(error) {
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable"
  );
});

add_task(async function test_subprocess_workdir() {
  let procDir = await OS.File.getCurrentDirectory();
  let tmpDirFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  tmpDirFile.initWithPath(OS.Constants.Path.tmpDir);
  tmpDirFile.normalize();
  let tmpDir = tmpDirFile.path;

  notEqual(
    procDir,
    tmpDir,
    "Current process directory must not be the current temp directory"
  );

  async function pwd(options) {
    let proc = await Subprocess.call(
      Object.assign(
        {
          command: PYTHON,
          arguments: ["-u", TEST_SCRIPT, "pwd"],
        },
        options
      )
    );

    let pwdOutput = read(proc.stdout);

    let { exitCode } = await proc.wait();
    equal(exitCode, 0, "Got expected exit code");

    return pwdOutput;
  }

  let dir = await pwd({});
  equal(
    dir,
    procDir,
    "Process should normally launch in current process directory"
  );

  dir = await pwd({ workdir: tmpDir });
  equal(
    dir,
    tmpDir,
    "Process should launch in the directory specified in `workdir`"
  );

  dir = await OS.File.getCurrentDirectory();
  equal(
    dir,
    procDir,
    "`workdir` should not change the working directory of the current process"
  );
});

add_task(async function test_subprocess_term() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // Windows does not support killing processes gracefully, so they will
  // always exit with -9 there.
  let retVal = AppConstants.platform == "win" ? -9 : -15;

  // Kill gracefully with the default timeout of 300ms.
  let { exitCode } = await proc.kill();

  equal(exitCode, retVal, "Got expected exit code");

  ({ exitCode } = await proc.wait());

  equal(exitCode, retVal, "Got expected exit code");
});

add_task(async function test_subprocess_kill() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "echo"],
  });

  // Force kill with no gracefull termination timeout.
  let { exitCode } = await proc.kill(0);

  equal(exitCode, -9, "Got expected exit code");

  ({ exitCode } = await proc.wait());

  equal(exitCode, -9, "Got expected exit code");
});

add_task(async function test_subprocess_kill_timeout() {
  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "ignore_sigterm"],
  });

  // Wait for the process to set up its signal handler and tell us it's
  // ready.
  let msg = await read(proc.stdout);
  equal(msg, "Ready", "Process is ready");

  // Kill gracefully with the default timeout of 300ms.
  // Expect a force kill after 300ms, since the process traps SIGTERM.
  const TIMEOUT = 300;
  let startTime = Date.now();

  let { exitCode } = await proc.kill(TIMEOUT);

  // Graceful termination is not supported on Windows, so don't bother
  // testing the timeout there.
  if (AppConstants.platform != "win") {
    let diff = Date.now() - startTime;
    ok(
      diff >= TIMEOUT,
      `Process was killed after ${diff}ms (expected ~${TIMEOUT}ms)`
    );
  }

  equal(exitCode, -9, "Got expected exit code");

  ({ exitCode } = await proc.wait());

  equal(exitCode, -9, "Got expected exit code");
});

add_task(async function test_subprocess_arguments() {
  let args = [
    String.raw`C:\Program Files\Company\Program.exe`,
    String.raw`\\NETWORK SHARE\Foo Directory${"\\"}`,
    String.raw`foo bar baz`,
    String.raw`"foo bar baz"`,
    String.raw`foo " bar`,
    String.raw`Thing \" with "" "\" \\\" \\\\" quotes\\" \\`,
  ];

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "print_args", ...args],
  });

  for (let [i, arg] of args.entries()) {
    let val = await read(proc.stdout);
    equal(val, arg, `Got correct value for args[${i}]`);
  }

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_environment() {
  let environment = {
    FOO: "BAR",
    EMPTY: "",
    IGNORED: null,
  };

  // Our Windows environment can't handle launching python without
  // PATH variables.
  if (AppConstants.platform == "win") {
    Object.assign(environment, {
      PATH: env.get("PATH"),
      PATHEXT: env.get("PATHEXT"),
      SYSTEMROOT: env.get("SYSTEMROOT"),
    });
  }

  env.set("BAR", "BAZ");

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: ["-u", TEST_SCRIPT, "env", "FOO", "BAR", "EMPTY", "IGNORED"],
    environment,
  });

  let foo = await read(proc.stdout);
  let bar = await read(proc.stdout);
  let empty = await read(proc.stdout);
  let ignored = await read(proc.stdout);

  equal(foo, "BAR", "Got expected $FOO value");
  equal(bar, "!", "Got expected $BAR value");
  equal(empty, "", "Got expected $EMPTY value");
  equal(ignored, "!", "Got expected $IGNORED value");

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");
});

add_task(async function test_subprocess_environmentAppend() {
  env.set("VALUE_FROM_BASE_ENV", "untouched");
  env.set("VALUE_FROM_BASE_ENV_EMPTY", "untouched");
  env.set("VALUE_FROM_BASE_ENV_REMOVED", "untouched");

  let proc = await Subprocess.call({
    command: PYTHON,
    arguments: [
      "-u",
      TEST_SCRIPT,
      "env",
      "VALUE_FROM_BASE_ENV",
      "VALUE_FROM_BASE_ENV_EMPTY",
      "VALUE_FROM_BASE_ENV_REMOVED",
      "VALUE_APPENDED_ONCE",
    ],
    environmentAppend: true,
    environment: {
      VALUE_FROM_BASE_ENV_EMPTY: "",
      VALUE_FROM_BASE_ENV_REMOVED: null,
      VALUE_APPENDED_ONCE: "soon empty",
    },
  });

  let valueFromBaseEnv = await read(proc.stdout);
  let valueFromBaseEnvEmpty = await read(proc.stdout);
  let valueFromBaseEnvRemoved = await read(proc.stdout);
  let valueAppendedOnce = await read(proc.stdout);

  equal(
    valueFromBaseEnv,
    "untouched",
    "Got expected $VALUE_FROM_BASE_ENV value"
  );
  equal(
    valueFromBaseEnvEmpty,
    "",
    "Got expected $VALUE_FROM_BASE_ENV_EMPTY value"
  );
  equal(
    valueFromBaseEnvRemoved,
    "!",
    "Got expected $VALUE_FROM_BASE_ENV_REMOVED value"
  );
  equal(
    valueAppendedOnce,
    "soon empty",
    "Got expected $VALUE_APPENDED_ONCE value"
  );

  let { exitCode } = await proc.wait();

  equal(exitCode, 0, "Got expected exit code");

  proc = await Subprocess.call({
    command: PYTHON,
    arguments: [
      "-u",
      TEST_SCRIPT,
      "env",
      "VALUE_FROM_BASE_ENV",
      "VALUE_APPENDED_ONCE",
    ],
    environmentAppend: true,
  });

  valueFromBaseEnv = await read(proc.stdout);
  valueAppendedOnce = await read(proc.stdout);

  equal(
    valueFromBaseEnv,
    "untouched",
    "Got expected $VALUE_FROM_BASE_ENV value"
  );
  equal(valueAppendedOnce, "!", "Got expected $VALUE_APPENDED_ONCE value");

  ({ exitCode } = await proc.wait());

  equal(exitCode, 0, "Got expected exit code");
});

if (AppConstants.platform !== "win") {
  add_task(async function test_subprocess_nonASCII() {
    const { libc } = ChromeUtils.import(
      "resource://gre/modules/subprocess/subprocess_unix.jsm",
      null
    );

    // Use TextDecoder rather than a string with a \xff escape, since
    // the latter will automatically be normalized to valid UTF-8.
    let val = new TextDecoder().decode(Uint8Array.of(1, 255));

    libc.setenv(
      "FOO",
      Uint8Array.from(val + "\0", c => c.charCodeAt(0)),
      1
    );

    let proc = await Subprocess.call({
      command: PYTHON,
      arguments: ["-u", TEST_SCRIPT, "env", "FOO"],
    });

    let foo = await read(proc.stdout);

    equal(foo, val, "Got expected $FOO value");

    env.set("FOO", "");

    let { exitCode } = await proc.wait();

    equal(exitCode, 0, "Got expected exit code");
  });
}

add_task(async function test_bad_executable() {
  // Test with a non-executable file.

  let textFile = do_get_file("data_text_file.txt").path;

  let promise = Subprocess.call({
    command: textFile,
    arguments: [],
  });

  await Assert.rejects(
    promise,
    function(error) {
      if (AppConstants.platform == "win") {
        return /Failed to create process/.test(error.message);
      }
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable"
  );

  // Test with a nonexistent file.
  promise = Subprocess.call({
    command: textFile + ".doesNotExist",
    arguments: [],
  });

  await Assert.rejects(
    promise,
    function(error) {
      return error.errorCode == Subprocess.ERROR_BAD_EXECUTABLE;
    },
    "Subprocess.call should fail for a bad executable"
  );
});

add_task(async function test_cleanup() {
  let { SubprocessImpl } = ChromeUtils.import(
    "resource://gre/modules/Subprocess.jsm",
    null
  );

  let worker = SubprocessImpl.Process.getWorker();

  let openFiles = await worker.call("getOpenFiles", []);
  let processes = await worker.call("getProcesses", []);

  equal(openFiles.size, 0, "No remaining open files");
  equal(processes.size, 0, "No remaining processes");
});
