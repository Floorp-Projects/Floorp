"use strict";

let envService = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

const PYTHON = envService.get("PYTHON");

const PYTHON_BIN = PathUtils.filename(PYTHON);
const PYTHON_DIR = PathUtils.parent(PYTHON);

const DOES_NOT_EXIST = PathUtils.join(
  PathUtils.osTempDir,
  "ThisPathDoesNotExist"
);

const PATH_SEP = AppConstants.platform == "win" ? ";" : ":";

add_task(async function test_pathSearchAbsolute() {
  let env = {};

  let path = await Subprocess.pathSearch(PYTHON, env);
  equal(path, PYTHON, "Full path resolves even with no PATH.");

  env.PATH = "";
  path = await Subprocess.pathSearch(PYTHON, env);
  equal(path, PYTHON, "Full path resolves even with empty PATH.");

  await Assert.rejects(
    Subprocess.pathSearch(DOES_NOT_EXIST, env),
    function(e) {
      equal(
        e.errorCode,
        Subprocess.ERROR_BAD_EXECUTABLE,
        "Got the expected error code"
      );
      return /File at path .* does not exist, or is not (executable|a normal file)/.test(
        e.message
      );
    },
    "Absolute path should throw for a nonexistent execuable"
  );
});

add_task(async function test_pathSearchRelative() {
  let env = {};

  await Assert.rejects(
    Subprocess.pathSearch(PYTHON_BIN, env),
    function(e) {
      equal(
        e.errorCode,
        Subprocess.ERROR_BAD_EXECUTABLE,
        "Got the expected error code"
      );
      return /Executable not found:/.test(e.message);
    },
    "Relative path should not be found when PATH is missing"
  );

  env.PATH = [DOES_NOT_EXIST, PYTHON_DIR].join(PATH_SEP);

  let path = await Subprocess.pathSearch(PYTHON_BIN, env);
  equal(path, PYTHON, "Correct executable should be found in the path");
});

add_task(
  {
    skip_if: () => AppConstants.platform != "win",
  },
  async function test_pathSearch_PATHEXT() {
    ok(PYTHON_BIN.endsWith(".exe"), "Python executable must end with .exe");

    const python_bin = PYTHON_BIN.slice(0, -4);

    let env = {
      PATH: PYTHON_DIR,
      PATHEXT: [".com", ".exe", ".foobar"].join(";"),
    };

    let path = await Subprocess.pathSearch(python_bin, env);
    equal(
      path,
      PYTHON,
      "Correct executable should be found in the path, with guessed extension"
    );
  }
);
// IMPORTANT: Do not add any tests beyond this point without removing
// the `skip_if` condition from the previous task, or it will prevent
// all succeeding tasks from running when it does not match.
