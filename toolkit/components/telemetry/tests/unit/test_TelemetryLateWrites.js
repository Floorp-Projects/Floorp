/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* A testcase to make sure reading late writes stacks works.  */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);

// Constants from prio.h for nsIFileOutputStream.init
const PR_WRONLY = 0x2;
const PR_CREATE_FILE = 0x8;
const PR_TRUNCATE = 0x20;
const RW_OWNER = parseInt("0600", 8);

const STACK_SUFFIX1 = "stack1.txt";
const STACK_SUFFIX2 = "stack2.txt";
const STACK_BOGUS_SUFFIX = "bogus.txt";
const LATE_WRITE_PREFIX = "Telemetry.LateWriteFinal-";

// The names and IDs don't matter, but the format of the IDs does.
const LOADED_MODULES = {
  "4759A7E6993548C89CAF716A67EC242D00": "libtest.so",
  F77AF15BB8D6419FA875954B4A3506CA00: "libxul.so",
  "1E2F7FB590424E8F93D60BB88D66B8C500": "libc.so",
  E4D6D70CC09A63EF8B88D532F867858800: "libmodμles.so",
};
const N_MODULES = Object.keys(LOADED_MODULES).length;

// Format of individual items is [index, offset-in-library].
const STACK1 = [
  [0, 0],
  [1, 1],
  [2, 2],
  [3, 3],
];
const STACK2 = [
  [0, 0],
  [1, 5],
  [2, 10],
  [3, 15],
];
// XXX The only error checking is for a zero-sized stack.
const STACK_BOGUS = [];

function write_string_to_file(file, contents) {
  let ostream = Cc[
    "@mozilla.org/network/safe-file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  ostream.init(
    file,
    PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
    RW_OWNER,
    ostream.DEFER_OPEN
  );

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(ostream);

  let utf8 = new TextEncoder("utf-8").encode(contents);
  bos.writeByteArray(utf8);
  ostream.QueryInterface(Ci.nsISafeOutputStream).finish();
  ostream.close();
}

function construct_file(suffix) {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append(LATE_WRITE_PREFIX + suffix);
  return file;
}

function write_late_writes_file(stack, suffix) {
  let file = construct_file(suffix);
  let contents = N_MODULES + "\n";
  for (let id in LOADED_MODULES) {
    contents += id + " " + LOADED_MODULES[id] + "\n";
  }

  contents += stack.length + "\n";
  for (let element of stack) {
    contents += element[0] + " " + element[1].toString(16) + "\n";
  }

  write_string_to_file(file, contents);
}

function run_test() {
  do_get_profile();

  write_late_writes_file(STACK1, STACK_SUFFIX1);
  write_late_writes_file(STACK2, STACK_SUFFIX2);
  write_late_writes_file(STACK_BOGUS, STACK_BOGUS_SUFFIX);

  let lateWrites = Telemetry.lateWrites;
  Assert.ok("memoryMap" in lateWrites);
  Assert.equal(lateWrites.memoryMap.length, 0);
  Assert.ok("stacks" in lateWrites);
  Assert.equal(lateWrites.stacks.length, 0);

  do_test_pending();
  Telemetry.asyncFetchTelemetryData(function() {
    actual_test();
  });
}

function actual_test() {
  Assert.ok(!construct_file(STACK_SUFFIX1).exists());
  Assert.ok(!construct_file(STACK_SUFFIX2).exists());
  Assert.ok(!construct_file(STACK_BOGUS_SUFFIX).exists());

  let lateWrites = Telemetry.lateWrites;

  Assert.ok("memoryMap" in lateWrites);
  Assert.equal(lateWrites.memoryMap.length, N_MODULES);
  for (let id in LOADED_MODULES) {
    let matchingLibrary = lateWrites.memoryMap.filter(function(
      library,
      idx,
      array
    ) {
      return library[1] == id;
    });
    Assert.equal(matchingLibrary.length, 1);
    let library = matchingLibrary[0];
    let name = library[0];
    Assert.equal(LOADED_MODULES[id], name);
  }

  Assert.ok("stacks" in lateWrites);
  Assert.equal(lateWrites.stacks.length, 2);
  let uneval_STACKS = [uneval(STACK1), uneval(STACK2)];
  let first_stack = lateWrites.stacks[0];
  let second_stack = lateWrites.stacks[1];
  function stackChecker(canonicalStack) {
    let unevalCanonicalStack = uneval(canonicalStack);
    return function(obj, idx, array) {
      return unevalCanonicalStack == obj;
    };
  }
  Assert.equal(uneval_STACKS.filter(stackChecker(first_stack)).length, 1);
  Assert.equal(uneval_STACKS.filter(stackChecker(second_stack)).length, 1);

  do_test_finished();
}
