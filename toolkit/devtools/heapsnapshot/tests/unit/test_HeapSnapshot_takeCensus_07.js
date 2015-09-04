/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// HeapSnapshot.prototype.takeCensus breakdown: check error handling on property
// gets.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-07.js

function run_test() {
  var g = newGlobal();
  var dbg = new Debugger(g);

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { get by() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");



  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'count', get count() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'count', get bytes() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");



  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'objectClass', get then() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'objectClass', get other() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");



  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'coarseType', get objects() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'coarseType', get scripts() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'coarseType', get strings() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'coarseType', get other() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");



  assertThrowsValue(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: 'internalType', get then() { throw "ಠ_ಠ" } }
    });
  }, "ಠ_ಠ");

  do_test_finished();
}
