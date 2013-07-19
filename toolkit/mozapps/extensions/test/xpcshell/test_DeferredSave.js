/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test behaviour of module to perform deferred save of data
// files to disk

"use strict";

const testFile = gProfD.clone();
testFile.append("DeferredSaveTest");

Components.utils.import("resource://gre/modules/Promise.jsm");

let context = Components.utils.import("resource://gre/modules/DeferredSave.jsm", {});
let DeferredSave = context.DeferredSave;

// Test wrapper to let us do promise/task based testing of DeferredSaveP
function DeferredSaveTester(aDelay, aDataProvider) {
  let tester = {
    // Deferred for the promise returned by the mock writeAtomic
    waDeferred: null,

    // The most recent data "written" by the mock OS.File.writeAtomic
    writtenData: undefined,

    dataToSave: "Data to save",

    save: (aData, aWriteHandler) => {
      tester.writeHandler = aWriteHandler || writer;
      tester.dataToSave = aData;
      return tester.saver.saveChanges();
    },

    flush: (aWriteHandler) => {
      tester.writeHandler = aWriteHandler || writer;
      return tester.saver.flush();
    },

    get error() {
      return tester.saver.error;
    }
  };

  // Default write handler for most cases where the test case doesn't need
  // to do anything while the write is in progress; just completes the write
  // on the next event loop
  function writer(aTester) {
    do_print("default write callback");
    let length = aTester.writtenData.length;
    do_execute_soon(() => aTester.waDeferred.resolve(length));
  }

  if (!aDataProvider)
    aDataProvider = () => tester.dataToSave;

  tester.saver = new DeferredSave(testFile.path, aDataProvider, aDelay);

  // Install a mock for OS.File.writeAtomic to let us control the async
  // behaviour of the promise
  context.OS.File.writeAtomic = function mock_writeAtomic(aFile, aData, aOptions) {
      do_print("writeAtomic: " + aFile + " data: '" + aData + "', " + aOptions.toSource());
      tester.writtenData = aData;
      tester.waDeferred = Promise.defer();
      tester.writeHandler(tester);
      return tester.waDeferred.promise;
    };

  return tester;
};

/**
 * Return a Promise<null> that resolves after the specified number of milliseconds
 */
function delay(aDelayMS) {
  let deferred = Promise.defer();
  do_timeout(aDelayMS, () => deferred.resolve(null));
  return deferred.promise;
}

function run_test() {
  run_next_test();
}

// Modify set data once, ask for save, make sure it saves cleanly
add_task(function test_basic_save_succeeds() {
  let tester = DeferredSaveTester(1);
  let data = "Test 1 Data";

  yield tester.save(data);
  do_check_eq(tester.writtenData, data);
  do_check_eq(1, tester.saver.totalSaves);
});

// Two saves called during the same event loop, both with callbacks
// Make sure we save only the second version of the data
add_task(function test_two_saves() {
  let tester = DeferredSaveTester(1);
  let firstCallback_happened = false;
  let firstData = "Test first save";
  let secondData = "Test second save";

  // first save should not resolve until after the second one is called,
  // so we can't just yield this promise
  tester.save(firstData).then(count => {
    do_check_eq(secondData, tester.writtenData);
    do_check_false(firstCallback_happened);
    firstCallback_happened = true;
  }, do_report_unexpected_exception);

  yield tester.save(secondData);
  do_check_true(firstCallback_happened);
  do_check_eq(secondData, tester.writtenData);
  do_check_eq(1, tester.saver.totalSaves);
});

// Two saves called with a delay in between, both with callbacks
// Make sure we save the second version of the data
add_task(function test_two_saves_delay() {
  let tester = DeferredSaveTester(50);
  let firstCallback_happened = false;
  let delayDone = false;

  let firstData = "First data to save with delay";
  let secondData = "Modified data to save with delay";

  tester.save(firstData).then(count => {
    do_check_false(firstCallback_happened);
    do_check_true(delayDone);
    do_check_eq(secondData, tester.writtenData);
    firstCallback_happened = true;
  }, do_report_unexpected_exception);

  yield delay(5);
  delayDone = true;
  yield tester.save(secondData);
  do_check_true(firstCallback_happened);
  do_check_eq(secondData, tester.writtenData);
  do_check_eq(1, tester.saver.totalSaves);
  do_check_eq(0, tester.saver.overlappedSaves);
});

// Test case where OS.File immediately reports an error when the write begins
// Also check that the "error" getter correctly returns the error
// Then do a write that succeeds, and make sure the error is cleared
add_task(function test_error_immediate() {
  let tester = DeferredSaveTester(1);
  let testError = new Error("Forced failure");
  function writeFail(aTester) {
    aTester.waDeferred.reject(testError);
  }

  yield tester.save("test_error_immediate", writeFail).then(
    count => do_throw("Did not get expected error"),
    error => do_check_eq(testError.message, error.message)
    );
  do_check_eq(testError, tester.error);

  // This write should succeed and clear the error
  yield tester.save("test_error_immediate succeeds");
  do_check_eq(null, tester.error);
  // The failed save attempt counts in our total
  do_check_eq(2, tester.saver.totalSaves);
});

// Save one set of changes, then while the write is in progress, modify the
// data two more times. Test that we re-write the dirty data exactly once
// after the first write succeeds
add_task(function dirty_while_writing() {
  let tester = DeferredSaveTester(1);
  let firstData = "First data";
  let secondData = "Second data";
  let thirdData = "Third data";
  let firstCallback_happened = false;
  let secondCallback_happened = false;
  let writeStarted = Promise.defer();

  function writeCallback(aTester) {
    writeStarted.resolve(aTester.waDeferred);
  }

  do_print("First save");
  tester.save(firstData, writeCallback).then(
    count => {
      do_check_false(firstCallback_happened);
      do_check_false(secondCallback_happened);
      do_check_eq(tester.writtenData, firstData);
      firstCallback_happened = true;
    }, do_report_unexpected_exception);

  do_print("waiting for writer");
  let writer = yield writeStarted.promise;
  do_print("Write started");

  // Delay a bit, modify the data and call saveChanges, delay a bit more,
  // modify the data and call saveChanges again, another delay,
  // then complete the in-progress write
  yield delay(1);

  tester.save(secondData).then(
    count => {
      do_check_true(firstCallback_happened);
      do_check_false(secondCallback_happened);
      do_check_eq(tester.writtenData, thirdData);
      secondCallback_happened = true;
    }, do_report_unexpected_exception);

  // wait and then do the third change
  yield delay(1);
  let thirdWrite = tester.save(thirdData);

  // wait a bit more and then finally finish the first write
  yield delay(1);
  writer.resolve(firstData.length);

  // Now let everything else finish
  yield thirdWrite;
  do_check_true(firstCallback_happened);
  do_check_true(secondCallback_happened);
  do_check_eq(tester.writtenData, thirdData);
  do_check_eq(2, tester.saver.totalSaves);
  do_check_eq(1, tester.saver.overlappedSaves);
});

// A write callback for the OS.File.writeAtomic mock that rejects write attempts
function disabled_write_callback(aTester) {
  do_throw("Should not have written during clean flush");
  deferred.reject(new Error("Write during supposedly clean flush"));
}

// special write callback that disables itself to make sure
// we don't try to write twice
function write_then_disable(aTester) {
  do_print("write_then_disable");
  let length = aTester.writtenData.length;
  aTester.writeHandler = disabled_write_callback;
  do_execute_soon(() => aTester.waDeferred.resolve(length));
}

// Flush tests. First, do an ordinary clean save and then call flush;
// there should not be another save
add_task(function flush_after_save() {
  let tester = DeferredSaveTester(1);
  let dataToSave = "Flush after save";

  yield tester.save(dataToSave);
  yield tester.flush(disabled_write_callback);
  do_check_eq(1, tester.saver.totalSaves);
});

// Flush while a write is in progress, but the in-memory data is clean
add_task(function flush_during_write() {
  let tester = DeferredSaveTester(1);
  let dataToSave = "Flush during write";
  let firstCallback_happened = false;
  let writeStarted = Promise.defer();

  function writeCallback(aTester) {
    writeStarted.resolve(aTester.waDeferred);
  }

  tester.save(dataToSave, writeCallback).then(
    count => {
      do_check_false(firstCallback_happened);
      firstCallback_happened = true;
    }, do_report_unexpected_exception);

  let writer = yield writeStarted.promise;

  // call flush with the write callback disabled, delay a bit more, complete in-progress write
  let flushing = tester.flush(disabled_write_callback);
  yield delay(2);
  writer.resolve(dataToSave.length);

  // now wait for the flush to finish
  yield flushing;
  do_check_true(firstCallback_happened);
  do_check_eq(1, tester.saver.totalSaves);
});

// Flush while dirty but write not in progress
// The data written should be the value at the time
// flush() is called, even if it is changed later
//
// It would be nice to have a mock for Timer in here, to control
// when the steps happen, but for now we'll call the flush without
// going back around the event loop to make sure it happens before
// the DeferredSave timer goes off
add_task(function flush_while_dirty() {
  let tester = DeferredSaveTester(20);
  let firstData = "Flush while dirty, valid data";
  let firstCallback_happened = false;

  tester.save(firstData, write_then_disable).then(
    count => {
      do_check_false(firstCallback_happened);
      firstCallback_happened = true;
      do_check_eq(tester.writtenData, firstData);
    }, do_report_unexpected_exception);

  // Also make sure that data changed after the flush call
  // (even without a saveChanges() call) doesn't get written
  let flushing = tester.flush();
  tester.dataToSave = "Flush while dirty, invalid data";
  yield flushing;
  do_check_true(firstCallback_happened);
  do_check_eq(tester.writtenData, firstData);
  do_check_eq(1, tester.saver.totalSaves);
});

// And the grand finale - modify the data, start writing,
// modify the data again so we're in progress and dirty,
// then flush, then modify the data again
// Data for the second write should be taken at the time
// flush() is called, even if it is modified later
add_task(function flush_writing_dirty() {
  let tester = DeferredSaveTester(5);
  let firstData = "Flush first pass data";
  let secondData = "Flush second pass data";
  let firstCallback_happened = false;
  let secondCallback_happened = false;
  let writeStarted = Promise.defer();

  function writeCallback(aTester) {
    writeStarted.resolve(aTester.waDeferred);
  }

  tester.save(firstData, writeCallback).then(
    count => {
      do_check_false(firstCallback_happened);
      do_check_eq(tester.writtenData, firstData);
      firstCallback_happened = true;
    }, do_report_unexpected_exception);

  let writer = yield writeStarted.promise;
  // the first write has started

  // dirty the data and request another save
  // after the second save completes, there should not be another write
  tester.save(secondData, write_then_disable).then(
    count => {
      do_check_true(firstCallback_happened);
      do_check_false(secondCallback_happened);
      do_check_eq(tester.writtenData, secondData);
      secondCallback_happened = true;
    }, do_report_unexpected_exception);

  let flushing = tester.flush(write_then_disable);
  tester.dataToSave = "Flush, invalid data: changed late";
  yield delay(1);
  // complete the first write
  writer.resolve(firstData.length);
  // now wait for the second write / flush to complete
  yield flushing;
  do_check_true(firstCallback_happened);
  do_check_true(secondCallback_happened);
  do_check_eq(tester.writtenData, secondData);
  do_check_eq(2, tester.saver.totalSaves);
  do_check_eq(1, tester.saver.overlappedSaves);
});

// A data provider callback that throws an error the first
// time it is called, and a different error the second time
// so that tests can (a) make sure the promise is rejected
// with the error and (b) make sure the provider is only
// called once in case of error
const expectedDataError = "Failed to serialize data";
let badDataError = null;
function badDataProvider() {
  let err = new Error(badDataError);
  badDataError = "badDataProvider called twice";
  throw err;
}

// Handle cases where data provider throws
// First, throws during a normal save
add_task(function data_throw() {
  badDataError = expectedDataError;
  let tester = DeferredSaveTester(1, badDataProvider);
  yield tester.save("data_throw").then(
    count => do_throw("Expected serialization failure"),
    error => do_check_eq(error.message, expectedDataError));
});

// Now, throws during flush
add_task(function data_throw_during_flush() {
  badDataError = expectedDataError;
  let tester = DeferredSaveTester(1, badDataProvider);
  let firstCallback_happened = false;

  // Write callback should never be called
  tester.save("data_throw_during_flush", disabled_write_callback).then(
    count => do_throw("Expected serialization failure"),
    error => {
      do_check_false(firstCallback_happened);
      do_check_eq(error.message, expectedDataError);
      firstCallback_happened = true;
    });

  yield tester.flush(disabled_write_callback).then(
    count => do_throw("Expected serialization failure"),
    error => do_check_eq(error.message, expectedDataError)
    );

  do_check_true(firstCallback_happened);
});

// Try to reproduce race condition. The observed sequence of events:
// saveChanges
// start writing
// saveChanges
// finish writing (need to restart delayed timer)
// saveChanges
// flush
// write starts
// actually restart timer for delayed write
// write completes
// delayed timer goes off, throws error because DeferredSave has been torn down
add_task(function delay_flush_race() {
  let tester = DeferredSaveTester(5);
  let firstData = "First save";
  let secondData = "Second save";
  let thirdData = "Third save";
  let writeStarted = Promise.defer();

  function writeCallback(aTester) {
    writeStarted.resolve(aTester.waDeferred);
  }

  // This promise won't resolve until after writeStarted
  let firstSave = tester.save(firstData, writeCallback);

  let writer = yield writeStarted.promise;
  // the first write has started

  // dirty the data and request another save
  let secondSave = tester.save(secondData);

  // complete the first write
  writer.resolve(firstData.length);
  yield firstSave;
  do_check_eq(tester.writtenData, firstData);

  tester.save(thirdData);
  let flushing = tester.flush();

  yield secondSave;
  do_check_eq(tester.writtenData, thirdData);

  yield flushing;
  do_check_eq(tester.writtenData, thirdData);

  // Our DeferredSave should not have a _timer here; if it
  // does, the bug caused a reschedule
  do_check_eq(null, tester.saver._timer);
});
