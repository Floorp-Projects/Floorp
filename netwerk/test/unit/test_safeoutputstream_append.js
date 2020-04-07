/* atomic-file-output-stream and safe-file-output-stream should throw and
 * exception if PR_APPEND is explicity specified without PR_TRUNCATE. */
"use strict";

const PR_WRONLY = 0x02;
const PR_CREATE_FILE = 0x08;
const PR_APPEND = 0x10;
const PR_TRUNCATE = 0x20;

function check_flag(file, contractID, flags, throws) {
  let stream = Cc[contractID].createInstance(Ci.nsIFileOutputStream);

  if (throws) {
    /* NS_ERROR_INVALID_ARG is reported as NS_ERROR_ILLEGAL_VALUE, since they
     * are same value. */
    Assert.throws(
      () => stream.init(file, flags, 0o644, 0),
      /NS_ERROR_ILLEGAL_VALUE/
    );
  } else {
    stream.init(file, flags, 0o644, 0);
    stream.close();
  }
}

function run_test() {
  let filename = "test.txt";
  let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
  file.append(filename);

  let tests = [
    [PR_WRONLY | PR_CREATE_FILE | PR_APPEND | PR_TRUNCATE, false],
    [PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, false],
    [PR_WRONLY | PR_CREATE_FILE | PR_APPEND, true],
    [-1, false],
  ];
  for (let contractID of [
    "@mozilla.org/network/atomic-file-output-stream;1",
    "@mozilla.org/network/safe-file-output-stream;1",
  ]) {
    for (let [flags, throws] of tests) {
      check_flag(file, contractID, flags, throws);
    }
  }
}
