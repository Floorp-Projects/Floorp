/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Ensures that data a request handler writes out in response is sent only as
 * quickly as the client can receive it, without racing ahead and being forced
 * to block while writing that data.
 *
 * NB: These tests are extremely tied to the current implementation, in terms of
 * when and how stream-ready notifications occur, the amount of data which will
 * be read or written at each notification, and so on.  If the implementation
 * changes in any way with respect to stream copying, this test will probably
 * have to change a little at the edges as well.
 */

gThreadManager = Cc["@mozilla.org/thread-manager;1"].createInstance();

function run_test() {
  do_test_pending();
  tests.push(function testsComplete(_) {
    dumpn(
      // eslint-disable-next-line no-useless-concat
      "******************\n" + "* TESTS COMPLETE *\n" + "******************"
    );
    do_test_finished();
  });

  runNextTest();
}

function runNextTest() {
  testIndex++;
  dumpn("*** runNextTest(), testIndex: " + testIndex);

  try {
    var test = tests[testIndex];
    test(runNextTest);
  } catch (e) {
    var msg = "exception running test " + testIndex + ": " + e;
    if (e && "stack" in e) {
      msg += "\nstack follows:\n" + e.stack;
    }
    do_throw(msg);
  }
}

/** ***********
 * TEST DATA *
 *************/

const NOTHING = [];

const FIRST_SEGMENT = [1, 2, 3, 4];
const SECOND_SEGMENT = [5, 6, 7, 8];
const THIRD_SEGMENT = [9, 10, 11, 12];

const SEGMENT = FIRST_SEGMENT;
const TWO_SEGMENTS = [1, 2, 3, 4, 5, 6, 7, 8];
const THREE_SEGMENTS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];

const SEGMENT_AND_HALF = [1, 2, 3, 4, 5, 6];

const QUARTER_SEGMENT = [1];
const HALF_SEGMENT = [1, 2];
const SECOND_HALF_SEGMENT = [3, 4];
const EXTRA_HALF_SEGMENT = [5, 6];
const MIDDLE_HALF_SEGMENT = [2, 3];
const LAST_QUARTER_SEGMENT = [4];
const HALF_THIRD_SEGMENT = [9, 10];
const LATTER_HALF_THIRD_SEGMENT = [11, 12];

const TWO_HALF_SEGMENTS = [1, 2, 1, 2];

/** *******
 * TESTS *
 *********/

var tests = [
  sourceClosedWithoutWrite,
  writeOneSegmentThenClose,
  simpleWriteThenRead,
  writeLittleBeforeReading,
  writeMultipleSegmentsThenRead,
  writeLotsBeforeReading,
  writeLotsBeforeReading2,
  writeThenReadPartial,
  manyPartialWrites,
  partialRead,
  partialWrite,
  sinkClosedImmediately,
  sinkClosedWithReadableData,
  sinkClosedAfterWrite,
  sourceAndSinkClosed,
  sinkAndSourceClosed,
  sourceAndSinkClosedWithPendingData,
  sinkAndSourceClosedWithPendingData,
];
var testIndex = -1;

function sourceClosedWithoutWrite(next) {
  var t = new CopyTest("sourceClosedWithoutWrite", next);

  t.closeSource(Cr.NS_OK);
  t.expect(Cr.NS_OK, [NOTHING]);
}

function writeOneSegmentThenClose(next) {
  var t = new CopyTest("writeLittleBeforeReading", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.closeSource(Cr.NS_OK);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.expect(Cr.NS_OK, [SEGMENT]);
}

function simpleWriteThenRead(next) {
  var t = new CopyTest("simpleWriteThenRead", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.closeSource(Cr.NS_OK);
  t.expect(Cr.NS_OK, [SEGMENT]);
}

function writeLittleBeforeReading(next) {
  var t = new CopyTest("writeLittleBeforeReading", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.closeSource(Cr.NS_OK);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.expect(Cr.NS_OK, [SEGMENT, SEGMENT]);
}

function writeMultipleSegmentsThenRead(next) {
  var t = new CopyTest("writeMultipleSegmentsThenRead", next);

  t.addToSource(TWO_SEGMENTS);
  t.makeSourceReadable(TWO_SEGMENTS.length);
  t.makeSinkWritableAndWaitFor(TWO_SEGMENTS.length, [
    FIRST_SEGMENT,
    SECOND_SEGMENT,
  ]);
  t.closeSource(Cr.NS_OK);
  t.expect(Cr.NS_OK, [TWO_SEGMENTS]);
}

function writeLotsBeforeReading(next) {
  var t = new CopyTest("writeLotsBeforeReading", next);

  t.addToSource(TWO_SEGMENTS);
  t.makeSourceReadable(TWO_SEGMENTS.length);
  t.makeSinkWritableAndWaitFor(FIRST_SEGMENT.length, [FIRST_SEGMENT]);
  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableAndWaitFor(SECOND_SEGMENT.length, [SECOND_SEGMENT]);
  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.closeSource(Cr.NS_OK);
  t.makeSinkWritableAndWaitFor(2 * SEGMENT.length, [SEGMENT, SEGMENT]);
  t.expect(Cr.NS_OK, [TWO_SEGMENTS, SEGMENT, SEGMENT]);
}

function writeLotsBeforeReading2(next) {
  var t = new CopyTest("writeLotsBeforeReading", next);

  t.addToSource(THREE_SEGMENTS);
  t.makeSourceReadable(THREE_SEGMENTS.length);
  t.makeSinkWritableAndWaitFor(FIRST_SEGMENT.length, [FIRST_SEGMENT]);
  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableAndWaitFor(SECOND_SEGMENT.length, [SECOND_SEGMENT]);
  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableAndWaitFor(THIRD_SEGMENT.length, [THIRD_SEGMENT]);
  t.closeSource(Cr.NS_OK);
  t.makeSinkWritableAndWaitFor(2 * SEGMENT.length, [SEGMENT, SEGMENT]);
  t.expect(Cr.NS_OK, [THREE_SEGMENTS, SEGMENT, SEGMENT]);
}

function writeThenReadPartial(next) {
  var t = new CopyTest("writeThenReadPartial", next);

  t.addToSource(SEGMENT_AND_HALF);
  t.makeSourceReadable(SEGMENT_AND_HALF.length);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.closeSource(Cr.NS_OK);
  t.makeSinkWritableAndWaitFor(EXTRA_HALF_SEGMENT.length, [EXTRA_HALF_SEGMENT]);
  t.expect(Cr.NS_OK, [SEGMENT_AND_HALF]);
}

function manyPartialWrites(next) {
  var t = new CopyTest("manyPartialWrites", next);

  t.addToSource(HALF_SEGMENT);
  t.makeSourceReadable(HALF_SEGMENT.length);

  t.addToSource(HALF_SEGMENT);
  t.makeSourceReadable(HALF_SEGMENT.length);
  t.makeSinkWritableAndWaitFor(2 * HALF_SEGMENT.length, [TWO_HALF_SEGMENTS]);
  t.closeSource(Cr.NS_OK);
  t.expect(Cr.NS_OK, [TWO_HALF_SEGMENTS]);
}

function partialRead(next) {
  var t = new CopyTest("partialRead", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.addToSource(HALF_SEGMENT);
  t.makeSourceReadable(HALF_SEGMENT.length);
  t.makeSinkWritableAndWaitFor(SEGMENT.length, [SEGMENT]);
  t.closeSourceAndWaitFor(Cr.NS_OK, HALF_SEGMENT.length, [HALF_SEGMENT]);
  t.expect(Cr.NS_OK, [SEGMENT, HALF_SEGMENT]);
}

function partialWrite(next) {
  var t = new CopyTest("partialWrite", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableByIncrementsAndWaitFor(SEGMENT.length, [
    QUARTER_SEGMENT,
    MIDDLE_HALF_SEGMENT,
    LAST_QUARTER_SEGMENT,
  ]);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.makeSinkWritableByIncrementsAndWaitFor(SEGMENT.length, [
    HALF_SEGMENT,
    SECOND_HALF_SEGMENT,
  ]);

  t.addToSource(THREE_SEGMENTS);
  t.makeSourceReadable(THREE_SEGMENTS.length);
  t.makeSinkWritableByIncrementsAndWaitFor(THREE_SEGMENTS.length, [
    HALF_SEGMENT,
    SECOND_HALF_SEGMENT,
    SECOND_SEGMENT,
    HALF_THIRD_SEGMENT,
    LATTER_HALF_THIRD_SEGMENT,
  ]);

  t.closeSource(Cr.NS_OK);
  t.expect(Cr.NS_OK, [SEGMENT, SEGMENT, THREE_SEGMENTS]);
}

function sinkClosedImmediately(next) {
  var t = new CopyTest("sinkClosedImmediately", next);

  t.closeSink(Cr.NS_OK);
  t.expect(Cr.NS_ERROR_UNEXPECTED, [NOTHING]);
}

function sinkClosedWithReadableData(next) {
  var t = new CopyTest("sinkClosedWithReadableData", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);
  t.closeSink(Cr.NS_OK);
  t.expect(Cr.NS_ERROR_UNEXPECTED, [NOTHING]);
}

function sinkClosedAfterWrite(next) {
  var t = new CopyTest("sinkClosedAfterWrite", next);

  t.addToSource(TWO_SEGMENTS);
  t.makeSourceReadable(TWO_SEGMENTS.length);
  t.makeSinkWritableAndWaitFor(FIRST_SEGMENT.length, [FIRST_SEGMENT]);
  t.closeSink(Cr.NS_OK);
  t.expect(Cr.NS_ERROR_UNEXPECTED, [FIRST_SEGMENT]);
}

function sourceAndSinkClosed(next) {
  var t = new CopyTest("sourceAndSinkClosed", next);

  t.closeSourceThenSink(Cr.NS_OK, Cr.NS_OK);
  t.expect(Cr.NS_OK, []);
}

function sinkAndSourceClosed(next) {
  var t = new CopyTest("sinkAndSourceClosed", next);

  t.closeSinkThenSource(Cr.NS_OK, Cr.NS_OK);

  // sink notify received first, hence error
  t.expect(Cr.NS_ERROR_UNEXPECTED, []);
}

function sourceAndSinkClosedWithPendingData(next) {
  var t = new CopyTest("sourceAndSinkClosedWithPendingData", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);

  t.closeSourceThenSink(Cr.NS_OK, Cr.NS_OK);

  // not all data from source copied, so error
  t.expect(Cr.NS_ERROR_UNEXPECTED, []);
}

function sinkAndSourceClosedWithPendingData(next) {
  var t = new CopyTest("sinkAndSourceClosedWithPendingData", next);

  t.addToSource(SEGMENT);
  t.makeSourceReadable(SEGMENT.length);

  t.closeSinkThenSource(Cr.NS_OK, Cr.NS_OK);

  // not all data from source copied, plus sink notify received first, so error
  t.expect(Cr.NS_ERROR_UNEXPECTED, []);
}

/** ***********
 * UTILITIES *
 *************/

/** Returns the sum of the elements in arr. */
function sum(arr) {
  var s = 0;
  for (var i = 0, sz = arr.length; i < sz; i++) {
    s += arr[i];
  }
  return s;
}

/**
 * Returns a constructor for an input or output stream callback that will wrap
 * the one provided to it as an argument.
 *
 * @param wrapperCallback : (nsIInputStreamCallback | nsIOutputStreamCallback) : void
 *   the original callback object (not a function!) being wrapped
 * @param name : string
 *   either "onInputStreamReady" if we're wrapping an input stream callback or
 *   "onOutputStreamReady" if we're wrapping an output stream callback
 * @returns function(nsIInputStreamCallback | nsIOutputStreamCallback) : (nsIInputStreamCallback | nsIOutputStreamCallback)
 *   a constructor function which constructs a callback object (not function!)
 *   which, when called, first calls the original callback provided to it and
 *   then calls wrapperCallback
 */
function createStreamReadyInterceptor(wrapperCallback, name) {
  return function StreamReadyInterceptor(callback) {
    this.wrappedCallback = callback;
    this[name] = function streamReadyInterceptor(stream) {
      dumpn("*** StreamReadyInterceptor." + name);

      try {
        dumpn("*** calling original " + name + "...");
        callback[name](stream);
      } catch (e) {
        dumpn("!!! error running inner callback: " + e);
        throw e;
      } finally {
        dumpn("*** calling wrapper " + name + "...");
        wrapperCallback[name](stream);
      }
    };
  };
}

/**
 * Print out a banner with the given message, uppercased, for debugging
 * purposes.
 */
function note(m) {
  m = m.toUpperCase();
  var asterisks = Array(m.length + 1 + 4).join("*");
  dumpn(asterisks + "\n* " + m + " *\n" + asterisks);
}

/** *********
 * MOCKERY *
 ***********/

/*
 * Blatantly violate abstractions in the name of testability.  THIS IS NOT
 * PUBLIC API!  If you use any of these I will knowingly break your code by
 * changing the names of variables and properties.
 */
// These are used in head.js.
/* exported BinaryInputStream, BinaryOutputStream */
var BinaryInputStream = function BIS(stream) {
  return stream;
};
var BinaryOutputStream = function BOS(stream) {
  return stream;
};
Response.SEGMENT_SIZE = SEGMENT.length;

/**
 * Roughly mocks an nsIPipe, presenting non-blocking input and output streams
 * that appear to also be binary streams and whose readability and writability
 * amounts are configurable.  Only the methods used in this test have been
 * implemented -- these aren't exact mocks (can't be, actually, because input
 * streams have unscriptable methods).
 *
 * @param name : string
 *   a name for this pipe, used in debugging output
 */
function CustomPipe(name) {
  var self = this;

  /** Data read from input that's buffered until it can be written to output. */
  this._data = [];

  /**
   * The status of this pipe, which is to say the error result the ends of this
   * pipe will return when attempts are made to use them.  This value is always
   * an error result when copying has finished, because success codes are
   * converted to NS_BASE_STREAM_CLOSED.
   */
  this._status = Cr.NS_OK;

  /** The input end of this pipe. */
  var input = (this.inputStream = {
    /** A name for this stream, used in debugging output. */
    name: name + " input",

    /**
     * The number of bytes of data available to be read from this pipe, or
     * Infinity if any amount of data in this pipe is made readable as soon as
     * it is written to the pipe output.
     */
    _readable: 0,

    /**
     * Data regarding a pending stream-ready callback on this, or null if no
     * callback is currently waiting to be called.
     */
    _waiter: null,

    /**
     * The event currently dispatched to make a stream-ready callback, if any
     * such callback is currently ready to be made and not already in
     * progress, or null when no callback is waiting to happen.
     */
    _event: null,

    /**
     * A stream-ready constructor to wrap an existing callback to intercept
     * stream-ready notifications, or null if notifications shouldn't be
     * wrapped at all.
     */
    _streamReadyInterceptCreator: null,

    /**
     * Registers a stream-ready wrapper creator function so that a
     * stream-ready callback made in the future can be wrapped.
     */
    interceptStreamReadyCallbacks(streamReadyInterceptCreator) {
      dumpn("*** [" + this.name + "].interceptStreamReadyCallbacks");

      Assert.ok(
        this._streamReadyInterceptCreator === null,
        "intercepting twice"
      );
      this._streamReadyInterceptCreator = streamReadyInterceptCreator;
      if (this._waiter) {
        this._waiter.callback = new streamReadyInterceptCreator(
          this._waiter.callback
        );
      }
    },

    /**
     * Removes a previously-registered stream-ready wrapper creator function,
     * also clearing any current wrapping.
     */
    removeStreamReadyInterceptor() {
      dumpn("*** [" + this.name + "].removeStreamReadyInterceptor()");

      Assert.ok(
        this._streamReadyInterceptCreator !== null,
        "removing interceptor when none present?"
      );
      this._streamReadyInterceptCreator = null;
      if (this._waiter) {
        this._waiter.callback = this._waiter.callback.wrappedCallback;
      }
    },

    //
    // see nsIAsyncInputStream.asyncWait
    //
    asyncWait: function asyncWait(callback, flags, requestedCount, target) {
      dumpn("*** [" + this.name + "].asyncWait");

      Assert.ok(callback && typeof callback !== "function");

      var closureOnly =
        (flags & Ci.nsIAsyncInputStream.WAIT_CLOSURE_ONLY) !== 0;

      Assert.ok(
        this._waiter === null || (this._waiter.closureOnly && !closureOnly),
        "asyncWait already called with a non-closure-only " +
          "callback?  unexpected!"
      );

      this._waiter = {
        callback: this._streamReadyInterceptCreator
          ? new this._streamReadyInterceptCreator(callback)
          : callback,
        closureOnly,
        requestedCount,
        eventTarget: target,
      };

      if (
        !Components.isSuccessCode(self._status) ||
        (!closureOnly &&
          this._readable >= requestedCount &&
          self._data.length >= requestedCount)
      ) {
        this._notify();
      }
    },

    //
    // see nsIAsyncInputStream.closeWithStatus
    //
    closeWithStatus: function closeWithStatus(status) {
      // eslint-disable-next-line no-useless-concat
      dumpn("*** [" + this.name + "].closeWithStatus" + "(" + status + ")");

      if (!Components.isSuccessCode(self._status)) {
        dumpn(
          "*** ignoring second closure of [input " +
            this.name +
            "] " +
            "(status " +
            self._status +
            ")"
        );
        return;
      }

      if (Components.isSuccessCode(status)) {
        status = Cr.NS_BASE_STREAM_CLOSED;
      }

      self._status = status;

      if (this._waiter) {
        this._notify();
      }
      if (output._waiter) {
        output._notify();
      }
    },

    //
    // see nsIBinaryInputStream.readByteArray
    //
    readByteArray: function readByteArray(count) {
      dumpn("*** [" + this.name + "].readByteArray(" + count + ")");

      if (self._data.length === 0) {
        throw Components.isSuccessCode(self._status)
          ? Cr.NS_BASE_STREAM_WOULD_BLOCK
          : self._status;
      }

      Assert.ok(
        this._readable <= self._data.length || this._readable === Infinity,
        "consistency check"
      );

      if (this._readable < count || self._data.length < count) {
        throw Components.Exception("", Cr.NS_BASE_STREAM_WOULD_BLOCK);
      }
      this._readable -= count;
      return self._data.splice(0, count);
    },

    /**
     * Makes the given number of additional bytes of data previously written
     * to the pipe's output stream available for reading, triggering future
     * notifications when required.
     *
     * @param count : uint
     *   the number of bytes of additional data to make available; must not be
     *   greater than the number of bytes already buffered but not made
     *   available by previous makeReadable calls
     */
    makeReadable: function makeReadable(count) {
      dumpn("*** [" + this.name + "].makeReadable(" + count + ")");

      Assert.ok(Components.isSuccessCode(self._status), "errant call");
      Assert.ok(
        this._readable + count <= self._data.length ||
          this._readable === Infinity,
        "increasing readable beyond written amount"
      );

      this._readable += count;

      dumpn("readable: " + this._readable + ", data: " + self._data);

      var waiter = this._waiter;
      if (waiter !== null) {
        if (waiter.requestedCount <= this._readable && !waiter.closureOnly) {
          this._notify();
        }
      }
    },

    /**
     * Disables the readability limit on this stream, meaning that as soon as
     * *any* amount of data is written to output it becomes available from
     * this stream and a stream-ready event is dispatched (if any stream-ready
     * callback is currently set).
     */
    disableReadabilityLimit: function disableReadabilityLimit() {
      dumpn("*** [" + this.name + "].disableReadabilityLimit()");

      this._readable = Infinity;
    },

    //
    // see nsIInputStream.available
    //
    available: function available() {
      dumpn("*** [" + this.name + "].available()");

      if (self._data.length === 0 && !Components.isSuccessCode(self._status)) {
        throw self._status;
      }

      return Math.min(this._readable, self._data.length);
    },

    /**
     * Dispatches a pending stream-ready event ahead of schedule, rather than
     * waiting for it to be dispatched in response to normal writes.  This is
     * useful when writing to the output has completed, and we need to have
     * read all data written to this stream.  If the output isn't closed and
     * the reading of data from this races ahead of the last write to output,
     * we need a notification to know when everything that's been written has
     * been read.  This ordinarily might be supplied by closing output, but
     * in some cases it's not desirable to close output, so this supplies an
     * alternative method to get notified when the last write has occurred.
     */
    maybeNotifyFinally: function maybeNotifyFinally() {
      dumpn("*** [" + this.name + "].maybeNotifyFinally()");

      Assert.ok(this._waiter !== null, "must be waiting now");

      if (self._data.length) {
        dumpn(
          "*** data still pending, normal notifications will signal " +
            "completion"
        );
        return;
      }

      // No data waiting to be written, so notify.  We could just close the
      // stream, but that's less faithful to the server's behavior (it doesn't
      // close the stream, and we're pretending to impersonate the server as
      // much as we can here), so instead we're going to notify when no data
      // can be read.  The CopyTest has already been flagged as complete, so
      // the stream listener will detect that this is a wrap-it-up notify and
      // invoke the next test.
      this._notify();
    },

    /**
     * Dispatches an event to call a previously-registered stream-ready
     * callback.
     */
    _notify: function _notify() {
      dumpn("*** [" + this.name + "]._notify()");

      var waiter = this._waiter;
      Assert.ok(waiter !== null, "no waiter?");

      if (this._event === null) {
        var event = (this._event = {
          run: function run() {
            input._waiter = null;
            input._event = null;
            try {
              Assert.ok(
                !Components.isSuccessCode(self._status) ||
                  input._readable >= waiter.requestedCount
              );
              waiter.callback.onInputStreamReady(input);
            } catch (e) {
              do_throw("error calling onInputStreamReady: " + e);
            }
          },
        });
        waiter.eventTarget.dispatch(event, Ci.nsIThread.DISPATCH_NORMAL);
      }
    },
  });

  /** The output end of this pipe. */
  var output = (this.outputStream = {
    /** A name for this stream, used in debugging output. */
    name: name + " output",

    /**
     * The number of bytes of data which may be written to this pipe without
     * blocking.
     */
    _writable: 0,

    /**
     * The increments in which pending data should be written, rather than
     * simply defaulting to the amount requested (which, given that
     * input.asyncWait precisely respects the requestedCount argument, will
     * ordinarily always be writable in that amount), as an array whose
     * elements from start to finish are the number of bytes to write each
     * time write() or writeByteArray() is subsequently called.  The sum of
     * the values in this array, if this array is not empty, is always equal
     * to this._writable.
     */
    _writableAmounts: [],

    /**
     * Data regarding a pending stream-ready callback on this, or null if no
     * callback is currently waiting to be called.
     */
    _waiter: null,

    /**
     * The event currently dispatched to make a stream-ready callback, if any
     * such callback is currently ready to be made and not already in
     * progress, or null when no callback is waiting to happen.
     */
    _event: null,

    /**
     * A stream-ready constructor to wrap an existing callback to intercept
     * stream-ready notifications, or null if notifications shouldn't be
     * wrapped at all.
     */
    _streamReadyInterceptCreator: null,

    /**
     * Registers a stream-ready wrapper creator function so that a
     * stream-ready callback made in the future can be wrapped.
     */
    interceptStreamReadyCallbacks(streamReadyInterceptCreator) {
      dumpn("*** [" + this.name + "].interceptStreamReadyCallbacks");

      Assert.ok(
        this._streamReadyInterceptCreator !== null,
        "intercepting onOutputStreamReady twice"
      );
      this._streamReadyInterceptCreator = streamReadyInterceptCreator;
      if (this._waiter) {
        this._waiter.callback = new streamReadyInterceptCreator(
          this._waiter.callback
        );
      }
    },

    /**
     * Removes a previously-registered stream-ready wrapper creator function,
     * also clearing any current wrapping.
     */
    removeStreamReadyInterceptor() {
      dumpn("*** [" + this.name + "].removeStreamReadyInterceptor()");

      Assert.ok(
        this._streamReadyInterceptCreator !== null,
        "removing interceptor when none present?"
      );
      this._streamReadyInterceptCreator = null;
      if (this._waiter) {
        this._waiter.callback = this._waiter.callback.wrappedCallback;
      }
    },

    //
    // see nsIAsyncOutputStream.asyncWait
    //
    asyncWait: function asyncWait(callback, flags, requestedCount, target) {
      dumpn("*** [" + this.name + "].asyncWait");

      Assert.ok(callback && typeof callback !== "function");

      var closureOnly =
        (flags & Ci.nsIAsyncInputStream.WAIT_CLOSURE_ONLY) !== 0;

      Assert.ok(
        this._waiter === null || (this._waiter.closureOnly && !closureOnly),
        "asyncWait already called with a non-closure-only " +
          "callback?  unexpected!"
      );

      this._waiter = {
        callback: this._streamReadyInterceptCreator
          ? new this._streamReadyInterceptCreator(callback)
          : callback,
        closureOnly,
        requestedCount,
        eventTarget: target,
        toString: function toString() {
          return (
            "waiter(" +
            (closureOnly ? "closure only, " : "") +
            "requestedCount: " +
            requestedCount +
            ", target: " +
            target +
            ")"
          );
        },
      };

      if (
        (!closureOnly && this._writable >= requestedCount) ||
        !Components.isSuccessCode(this.status)
      ) {
        this._notify();
      }
    },

    //
    // see nsIAsyncOutputStream.closeWithStatus
    //
    closeWithStatus: function closeWithStatus(status) {
      dumpn("*** [" + this.name + "].closeWithStatus(" + status + ")");

      if (!Components.isSuccessCode(self._status)) {
        dumpn(
          "*** ignoring redundant closure of [input " +
            this.name +
            "] " +
            "because it's already closed (status " +
            self._status +
            ")"
        );
        return;
      }

      if (Components.isSuccessCode(status)) {
        status = Cr.NS_BASE_STREAM_CLOSED;
      }

      self._status = status;

      if (input._waiter) {
        input._notify();
      }
      if (this._waiter) {
        this._notify();
      }
    },

    //
    // see nsIBinaryOutputStream.writeByteArray
    //
    writeByteArray: function writeByteArray(bytes) {
      dumpn(`*** [${this.name}].writeByteArray([${bytes}])`);

      if (!Components.isSuccessCode(self._status)) {
        throw self._status;
      }

      Assert.equal(
        this._writableAmounts.length,
        0,
        "writeByteArray can't support specified-length writes"
      );

      if (this._writable < bytes.length) {
        throw Components.Exception("", Cr.NS_BASE_STREAM_WOULD_BLOCK);
      }

      self._data.push.apply(self._data, bytes);
      this._writable -= bytes.length;

      if (
        input._readable === Infinity &&
        input._waiter &&
        !input._waiter.closureOnly
      ) {
        input._notify();
      }
    },

    //
    // see nsIOutputStream.write
    //
    write: function write(str, length) {
      dumpn("*** [" + this.name + "].write");

      Assert.equal(str.length, length, "sanity");
      if (!Components.isSuccessCode(self._status)) {
        throw self._status;
      }
      if (this._writable === 0) {
        throw Components.Exception("", Cr.NS_BASE_STREAM_WOULD_BLOCK);
      }

      var actualWritten;
      if (this._writableAmounts.length === 0) {
        actualWritten = Math.min(this._writable, length);
      } else {
        Assert.ok(
          this._writable >= this._writableAmounts[0],
          "writable amounts value greater than writable data?"
        );
        Assert.equal(
          this._writable,
          sum(this._writableAmounts),
          "total writable amount not equal to sum of writable increments"
        );
        actualWritten = this._writableAmounts.shift();
      }

      var bytes = str
        .substring(0, actualWritten)
        .split("")
        .map(function(v) {
          return v.charCodeAt(0);
        });

      self._data.push.apply(self._data, bytes);
      this._writable -= actualWritten;

      if (
        input._readable === Infinity &&
        input._waiter &&
        !input._waiter.closureOnly
      ) {
        input._notify();
      }

      return actualWritten;
    },

    /**
     * Increase the amount of data that can be written without blocking by the
     * given number of bytes, triggering future notifications when required.
     *
     * @param count : uint
     *   the number of bytes of additional data to make writable
     */
    makeWritable: function makeWritable(count) {
      dumpn("*** [" + this.name + "].makeWritable(" + count + ")");

      Assert.ok(Components.isSuccessCode(self._status));

      this._writable += count;

      var waiter = this._waiter;
      if (
        waiter &&
        !waiter.closureOnly &&
        waiter.requestedCount <= this._writable
      ) {
        this._notify();
      }
    },

    /**
     * Increase the amount of data that can be written without blocking, but
     * do so by specifying a number of bytes that will be written each time
     * a write occurs, even as asyncWait notifications are initially triggered
     * as usual.  Thus, rather than writes eagerly writing everything possible
     * at each step, attempts to write out data by segment devolve into a
     * partial segment write, then another, and so on until the amount of data
     * specified as permitted to be written, has been written.
     *
     * Note that the writeByteArray method is incompatible with the previous
     * calling of this method, in that, until all increments provided to this
     * method have been consumed, writeByteArray cannot be called.  Once all
     * increments have been consumed, writeByteArray may again be called.
     *
     * @param increments : [uint]
     *   an array whose elements are positive numbers of bytes to permit to be
     *   written each time write() is subsequently called on this, ignoring
     *   the total amount of writable space specified by the sum of all
     *   increments
     */
    makeWritableByIncrements: function makeWritableByIncrements(increments) {
      dumpn(
        "*** [" +
          this.name +
          "].makeWritableByIncrements" +
          "([" +
          increments.join(", ") +
          "])"
      );

      Assert.greater(increments.length, 0, "bad increments");
      Assert.ok(
        increments.every(function(v) {
          return v > 0;
        }),
        "zero increment?"
      );

      Assert.ok(Components.isSuccessCode(self._status));

      this._writable += sum(increments);
      this._writableAmounts = increments;

      var waiter = this._waiter;
      if (
        waiter &&
        !waiter.closureOnly &&
        waiter.requestedCount <= this._writable
      ) {
        this._notify();
      }
    },

    /**
     * Dispatches an event to call a previously-registered stream-ready
     * callback.
     */
    _notify: function _notify() {
      dumpn("*** [" + this.name + "]._notify()");

      var waiter = this._waiter;
      Assert.ok(waiter !== null, "no waiter?");

      if (this._event === null) {
        var event = (this._event = {
          run: function run() {
            output._waiter = null;
            output._event = null;

            try {
              waiter.callback.onOutputStreamReady(output);
            } catch (e) {
              do_throw("error calling onOutputStreamReady: " + e);
            }
          },
        });
        waiter.eventTarget.dispatch(event, Ci.nsIThread.DISPATCH_NORMAL);
      }
    },
  });
}

/**
 * Represents a sequence of interactions to perform with a copier, in a given
 * order and at the desired time intervals.
 *
 * @param name : string
 *   test name, used in debugging output
 */
function CopyTest(name, next) {
  /** Name used in debugging output. */
  this.name = name;

  /** A function called when the test completes. */
  this._done = next;

  var sourcePipe = new CustomPipe(name + "-source");

  /** The source of data for the copier to copy. */
  this._source = sourcePipe.inputStream;

  /**
   * The sink to which to write data which will appear in the copier's source.
   */
  this._copyableDataStream = sourcePipe.outputStream;

  var sinkPipe = new CustomPipe(name + "-sink");

  /** The sink to which the copier copies data. */
  this._sink = sinkPipe.outputStream;

  /** Input stream from which to read data the copier's written to its sink. */
  this._copiedDataStream = sinkPipe.inputStream;

  this._copiedDataStream.disableReadabilityLimit();

  /**
   * True if there's a callback waiting to read data written by the copier to
   * its output, from the input end of the pipe representing the copier's sink.
   */
  this._waitingForData = false;

  /**
   * An array of the bytes of data expected to be written to output by the
   * copier when this test runs.
   */
  this._expectedData = undefined;

  /** Array of bytes of data received so far. */
  this._receivedData = [];

  /** The expected final status returned by the copier. */
  this._expectedStatus = -1;

  /** The actual final status returned by the copier. */
  this._actualStatus = -1;

  /** The most recent sequence of bytes written to output by the copier. */
  this._lastQuantum = [];

  /**
   * True iff we've received the last quantum of data written to the sink by the
   * copier.
   */
  this._allDataWritten = false;

  /**
   * True iff the copier has notified its associated stream listener of
   * completion.
   */
  this._copyingFinished = false;

  /** Index of the next task to execute while driving the copier. */
  this._currentTask = 0;

  /** Array containing all tasks to run. */
  this._tasks = [];

  /** The copier used by this test. */
  this._copier = new WriteThroughCopier(this._source, this._sink, this, null);

  // Start watching for data written by the copier to the sink.
  this._waitForWrittenData();
}
CopyTest.prototype = {
  /**
   * Adds the given array of bytes to data in the copier's source.
   *
   * @param bytes : [uint]
   *   array of bytes of data to add to the source for the copier
   */
  addToSource: function addToSource(bytes) {
    var self = this;
    this._addToTasks(function addToSourceTask() {
      note("addToSourceTask");

      try {
        self._copyableDataStream.makeWritable(bytes.length);
        self._copyableDataStream.writeByteArray(bytes);
      } finally {
        self._stageNextTask();
      }
    });
  },

  /**
   * Makes bytes of data previously added to the source available to be read by
   * the copier.
   *
   * @param count : uint
   *   number of bytes to make available for reading
   */
  makeSourceReadable: function makeSourceReadable(count) {
    var self = this;
    this._addToTasks(function makeSourceReadableTask() {
      note("makeSourceReadableTask");

      self._source.makeReadable(count);
      self._stageNextTask();
    });
  },

  /**
   * Increases available space in the sink by the given amount, waits for the
   * given series of arrays of bytes to be written to sink by the copier, and
   * causes execution to asynchronously continue to the next task when the last
   * of those arrays of bytes is received.
   *
   * @param bytes : uint
   *   number of bytes of space to make available in the sink
   * @param dataQuantums : [[uint]]
   *   array of byte arrays to expect to be written in sequence to the sink
   */
  makeSinkWritableAndWaitFor: function makeSinkWritableAndWaitFor(
    bytes,
    dataQuantums
  ) {
    var self = this;

    Assert.equal(
      bytes,
      dataQuantums.reduce(function(partial, current) {
        return partial + current.length;
      }, 0),
      "bytes/quantums mismatch"
    );

    function increaseSinkSpaceTask() {
      /* Now do the actual work to trigger the interceptor. */
      self._sink.makeWritable(bytes);
    }

    this._waitForHelper(
      "increaseSinkSpaceTask",
      dataQuantums,
      increaseSinkSpaceTask
    );
  },

  /**
   * Increases available space in the sink by the given amount, waits for the
   * given series of arrays of bytes to be written to sink by the copier, and
   * causes execution to asynchronously continue to the next task when the last
   * of those arrays of bytes is received.
   *
   * @param bytes : uint
   *   number of bytes of space to make available in the sink
   * @param dataQuantums : [[uint]]
   *   array of byte arrays to expect to be written in sequence to the sink
   */
  makeSinkWritableByIncrementsAndWaitFor: function makeSinkWritableByIncrementsAndWaitFor(
    bytes,
    dataQuantums
  ) {
    var self = this;

    var desiredAmounts = dataQuantums.map(function(v) {
      return v.length;
    });
    Assert.equal(bytes, sum(desiredAmounts), "bytes/quantums mismatch");

    function increaseSinkSpaceByIncrementsTask() {
      /* Now do the actual work to trigger the interceptor incrementally. */
      self._sink.makeWritableByIncrements(desiredAmounts);
    }

    this._waitForHelper(
      "increaseSinkSpaceByIncrementsTask",
      dataQuantums,
      increaseSinkSpaceByIncrementsTask
    );
  },

  /**
   * Close the copier's source stream, then asynchronously continue to the next
   * task.
   *
   * @param status : nsresult
   *   the status to provide when closing the copier's source stream
   */
  closeSource: function closeSource(status) {
    var self = this;

    this._addToTasks(function closeSourceTask() {
      note("closeSourceTask");

      self._source.closeWithStatus(status);
      self._stageNextTask();
    });
  },

  /**
   * Close the copier's source stream, then wait for the given number of bytes
   * and for the given series of arrays of bytes to be written to the sink, then
   * asynchronously continue to the next task.
   *
   * @param status : nsresult
   *   the status to provide when closing the copier's source stream
   * @param bytes : uint
   *   number of bytes of space to make available in the sink
   * @param dataQuantums : [[uint]]
   *   array of byte arrays to expect to be written in sequence to the sink
   */
  closeSourceAndWaitFor: function closeSourceAndWaitFor(
    status,
    bytes,
    dataQuantums
  ) {
    var self = this;

    Assert.equal(
      bytes,
      sum(
        dataQuantums.map(function(v) {
          return v.length;
        })
      ),
      "bytes/quantums mismatch"
    );

    function closeSourceAndWaitForTask() {
      self._sink.makeWritable(bytes);
      self._copyableDataStream.closeWithStatus(status);
    }

    this._waitForHelper(
      "closeSourceAndWaitForTask",
      dataQuantums,
      closeSourceAndWaitForTask
    );
  },

  /**
   * Closes the copier's sink stream, providing the given status, then
   * asynchronously continue to the next task.
   *
   * @param status : nsresult
   *   the status to provide when closing the copier's sink stream
   */
  closeSink: function closeSink(status) {
    var self = this;
    this._addToTasks(function closeSinkTask() {
      note("closeSinkTask");

      self._sink.closeWithStatus(status);
      self._stageNextTask();
    });
  },

  /**
   * Closes the copier's source stream, then immediately closes the copier's
   * sink stream, then asynchronously continues to the next task.
   *
   * @param sourceStatus : nsresult
   *   the status to provide when closing the copier's source stream
   * @param sinkStatus : nsresult
   *   the status to provide when closing the copier's sink stream
   */
  closeSourceThenSink: function closeSourceThenSink(sourceStatus, sinkStatus) {
    var self = this;
    this._addToTasks(function closeSourceThenSinkTask() {
      note("closeSourceThenSinkTask");

      self._source.closeWithStatus(sourceStatus);
      self._sink.closeWithStatus(sinkStatus);
      self._stageNextTask();
    });
  },

  /**
   * Closes the copier's sink stream, then immediately closes the copier's
   * source stream, then asynchronously continues to the next task.
   *
   * @param sinkStatus : nsresult
   *   the status to provide when closing the copier's sink stream
   * @param sourceStatus : nsresult
   *   the status to provide when closing the copier's source stream
   */
  closeSinkThenSource: function closeSinkThenSource(sinkStatus, sourceStatus) {
    var self = this;
    this._addToTasks(function closeSinkThenSourceTask() {
      note("closeSinkThenSource");

      self._sink.closeWithStatus(sinkStatus);
      self._source.closeWithStatus(sourceStatus);
      self._stageNextTask();
    });
  },

  /**
   * Indicates that the given status is expected to be returned when the stream
   * listener for the copy indicates completion, that the expected data copied
   * by the copier to sink are the concatenation of the arrays of bytes in
   * receivedData, and kicks off the tasks in this test.
   *
   * @param expectedStatus : nsresult
   *   the status expected to be returned by the copier at completion
   * @param receivedData : [[uint]]
   *   an array containing arrays of bytes whose concatenation constitutes the
   *   expected copied data
   */
  expect: function expect(expectedStatus, receivedData) {
    this._expectedStatus = expectedStatus;
    this._expectedData = [];
    for (var i = 0, sz = receivedData.length; i < sz; i++) {
      this._expectedData.push.apply(this._expectedData, receivedData[i]);
    }

    this._stageNextTask();
  },

  /**
   * Sets up a stream interceptor that will verify that each piece of data
   * written to the sink by the copier corresponds to the currently expected
   * pieces of data, calls the trigger, then waits for those pieces of data to
   * be received.  Once all have been received, the interceptor is removed and
   * the next task is asynchronously executed.
   *
   * @param name : string
   *   name of the task created by this, used in debugging output
   * @param dataQuantums : [[uint]]
   *   array of expected arrays of bytes to be written to the sink by the copier
   * @param trigger : function() : void
   *   function to call after setting up the interceptor to wait for
   *   notifications (which will be generated as a result of this function's
   *   actions)
   */
  _waitForHelper: function _waitForHelper(name, dataQuantums, trigger) {
    var self = this;
    this._addToTasks(function waitForHelperTask() {
      note(name);

      var quantumIndex = 0;

      /*
       * Intercept all data-available notifications so we can continue when all
       * the ones we expect have been received.
       */
      var streamReadyCallback = {
        onInputStreamReady: function wrapperOnInputStreamReady(input) {
          dumpn(
            "*** streamReadyCallback.onInputStreamReady" +
              "(" +
              input.name +
              ")"
          );

          Assert.equal(this, streamReadyCallback, "sanity");

          try {
            if (quantumIndex < dataQuantums.length) {
              var quantum = dataQuantums[quantumIndex++];
              var sz = quantum.length;
              Assert.equal(
                self._lastQuantum.length,
                sz,
                "different quantum lengths"
              );
              for (var i = 0; i < sz; i++) {
                Assert.equal(
                  self._lastQuantum[i],
                  quantum[i],
                  "bad data at " + i
                );
              }

              dumpn(
                "*** waiting to check remaining " +
                  (dataQuantums.length - quantumIndex) +
                  " quantums..."
              );
            }
          } finally {
            if (quantumIndex === dataQuantums.length) {
              dumpn("*** data checks completed!  next task...");
              self._copiedDataStream.removeStreamReadyInterceptor();
              self._stageNextTask();
            }
          }
        },
      };

      var interceptor = createStreamReadyInterceptor(
        streamReadyCallback,
        "onInputStreamReady"
      );
      self._copiedDataStream.interceptStreamReadyCallbacks(interceptor);

      /* Do the deed. */
      trigger();
    });
  },

  /**
   * Initiates asynchronous waiting for data written to the copier's sink to be
   * available for reading from the input end of the sink's pipe.  The callback
   * stores the received data for comparison in the interceptor used in the
   * callback added by _waitForHelper and signals test completion when it
   * receives a zero-data-available notification (if the copier has notified
   * that it is finished; otherwise allows execution to continue until that has
   * occurred).
   */
  _waitForWrittenData: function _waitForWrittenData() {
    dumpn("*** _waitForWrittenData (" + this.name + ")");

    var self = this;
    var outputWrittenWatcher = {
      onInputStreamReady: function onInputStreamReady(input) {
        dumpn(
          // eslint-disable-next-line no-useless-concat
          "*** outputWrittenWatcher.onInputStreamReady" + "(" + input.name + ")"
        );

        if (self._allDataWritten) {
          do_throw(
            "ruh-roh!  why are we getting notified of more data " +
              "after we should have received all of it?"
          );
        }

        self._waitingForData = false;

        try {
          var avail = input.available();
        } catch (e) {
          dumpn("*** available() threw!  error: " + e);
          if (self._completed) {
            dumpn(
              "*** NB: this isn't a problem, because we've copied " +
                "completely now, and this notify may have been expedited " +
                "by maybeNotifyFinally such that we're being called when " +
                "we can *guarantee* nothing is available any more"
            );
          }
          avail = 0;
        }

        if (avail > 0) {
          var data = input.readByteArray(avail);
          Assert.equal(
            data.length,
            avail,
            "readByteArray returned wrong number of bytes?"
          );
          self._lastQuantum = data;
          self._receivedData.push.apply(self._receivedData, data);
        }

        if (avail === 0) {
          dumpn("*** all data received!");

          self._allDataWritten = true;

          if (self._copyingFinished) {
            dumpn("*** copying already finished, continuing to next test");
            self._testComplete();
          } else {
            dumpn("*** copying not finished, waiting for that to happen");
          }

          return;
        }

        self._waitForWrittenData();
      },
    };

    this._copiedDataStream.asyncWait(
      outputWrittenWatcher,
      0,
      1,
      gThreadManager.currentThread
    );
    this._waitingForData = true;
  },

  /**
   * Indicates this test is complete, does the final data-received and copy
   * status comparisons, and calls the test-completion function provided when
   * this test was first created.
   */
  _testComplete: function _testComplete() {
    dumpn("*** CopyTest(" + this.name + ") complete!  On to the next test...");

    try {
      Assert.ok(this._allDataWritten, "expect all data written now!");
      Assert.ok(this._copyingFinished, "expect copying finished now!");

      Assert.equal(
        this._actualStatus,
        this._expectedStatus,
        "wrong final status"
      );

      var expected = this._expectedData,
        received = this._receivedData;
      dumpn("received: [" + received + "], expected: [" + expected + "]");
      Assert.equal(received.length, expected.length, "wrong data");
      for (var i = 0, sz = expected.length; i < sz; i++) {
        Assert.equal(received[i], expected[i], "bad data at " + i);
      }
    } catch (e) {
      dumpn("!!! ERROR PERFORMING FINAL " + this.name + " CHECKS!  " + e);
      throw e;
    } finally {
      dumpn(
        "*** CopyTest(" +
          this.name +
          ") complete!  " +
          "Invoking test-completion callback..."
      );
      this._done();
    }
  },

  /** Dispatches an event at this thread which will run the next task. */
  _stageNextTask: function _stageNextTask() {
    dumpn("*** CopyTest(" + this.name + ")._stageNextTask()");

    if (this._currentTask === this._tasks.length) {
      dumpn("*** CopyTest(" + this.name + ") tasks complete!");
      return;
    }

    var task = this._tasks[this._currentTask++];
    var event = {
      run: function run() {
        try {
          task();
        } catch (e) {
          do_throw("exception thrown running task: " + e);
        }
      },
    };
    gThreadManager.dispatchToMainThread(event);
  },

  /**
   * Adds the given function as a task to be run at a later time.
   *
   * @param task : function() : void
   *   the function to call as a task
   */
  _addToTasks: function _addToTasks(task) {
    this._tasks.push(task);
  },

  //
  // see nsIRequestObserver.onStartRequest
  //
  onStartRequest: function onStartRequest(self) {
    dumpn("*** CopyTest.onStartRequest (" + self.name + ")");

    Assert.equal(this._receivedData.length, 0);
    Assert.equal(this._lastQuantum.length, 0);
  },

  //
  // see nsIRequestObserver.onStopRequest
  //
  onStopRequest: function onStopRequest(self, status) {
    dumpn("*** CopyTest.onStopRequest (" + self.name + ", " + status + ")");

    this._actualStatus = status;

    this._copyingFinished = true;

    if (this._allDataWritten) {
      dumpn("*** all data written, continuing with remaining tests...");
      this._testComplete();
    } else {
      /*
       * Everything's copied as far as the copier is concerned.  However, there
       * may be a backup transferring from the output end of the copy sink to
       * the input end where we can actually verify that the expected data was
       * written as expected, because that transfer occurs asynchronously.  If
       * we do final data-received checks now, we'll miss still-pending data.
       * Therefore, to wrap up this copy test we still need to asynchronously
       * wait on the input end of the sink until we hit end-of-stream or some
       * error condition.  Then we know we're done and can continue with the
       * next test.
       */
      dumpn("*** not all data copied, waiting for that to happen...");

      if (!this._waitingForData) {
        this._waitForWrittenData();
      }

      this._copiedDataStream.maybeNotifyFinally();
    }
  },
};
