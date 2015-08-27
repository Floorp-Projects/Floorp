/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can read core dumps into HeapSnapshot instances.

if (typeof Debugger != "function") {
  const { addDebuggerToGlobal } = Cu.import("resource://gre/modules/jsdebugger.jsm", {});
  addDebuggerToGlobal(this);
}

function run_test() {
  const filePath = getFilePath("core-dump-" + Math.random() + ".tmp", true, true);
  ok(filePath, "Should get a file path");

  ChromeUtils.saveHeapSnapshot(filePath, { globals: [this] });
  ok(true, "Should be able to save a snapshot.");

  const snapshot = ChromeUtils.readHeapSnapshot(filePath);
  ok(snapshot, "Should be able to read a heap snapshot");
  ok(snapshot instanceof HeapSnapshot, "Should be an instanceof HeapSnapshot");

  do_test_finished();
}
