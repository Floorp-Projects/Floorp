"use strict";

var CC = Components.Constructor;

var Pipe = CC("@mozilla.org/pipe;1", Ci.nsIPipe, "init");
var BufferedOutputStream = CC(
  "@mozilla.org/network/buffered-output-stream;1",
  Ci.nsIBufferedOutputStream,
  "init"
);
var ScriptableInputStream = CC(
  "@mozilla.org/scriptableinputstream;1",
  Ci.nsIScriptableInputStream,
  "init"
);

// Verify that pipes behave as we expect. Subsequent tests assume
// pipes behave as demonstrated here.
add_test(function checkWouldBlockPipe() {
  // Create a pipe with a one-byte buffer
  var pipe = new Pipe(true, true, 1, 1);

  // Writing two bytes should transfer only one byte, and
  // return a partial count, not would-block.
  Assert.equal(pipe.outputStream.write("xy", 2), 1);
  Assert.equal(pipe.inputStream.available(), 1);

  do_check_throws_nsIException(
    () => pipe.outputStream.write("y", 1),
    "NS_BASE_STREAM_WOULD_BLOCK"
  );

  // Check that nothing was written to the pipe.
  Assert.equal(pipe.inputStream.available(), 1);
  run_next_test();
});

// A writeFrom to a buffered stream should return
// NS_BASE_STREAM_WOULD_BLOCK if no data was written.
add_test(function writeFromBlocksImmediately() {
  // Create a full pipe for our output stream. This will 'would-block' when
  // written to.
  var outPipe = new Pipe(true, true, 1, 1);
  Assert.equal(outPipe.outputStream.write("x", 1), 1);

  // Create a buffered stream, and fill its buffer, so the next write will
  // try to flush.
  var buffered = new BufferedOutputStream(outPipe.outputStream, 10);
  Assert.equal(buffered.write("0123456789", 10), 10);

  // Create a pipe with some data to be our input stream for the writeFrom
  // call.
  var inPipe = new Pipe(true, true, 1, 1);
  Assert.equal(inPipe.outputStream.write("y", 1), 1);

  Assert.equal(inPipe.inputStream.available(), 1);
  do_check_throws_nsIException(
    () => buffered.writeFrom(inPipe.inputStream, 1),
    "NS_BASE_STREAM_WOULD_BLOCK"
  );

  // No data should have been consumed from the pipe.
  Assert.equal(inPipe.inputStream.available(), 1);

  run_next_test();
});

// A writeFrom to a buffered stream should return a partial count if any
// data is written, when the last Flush call can only flush a portion of
// the data.
add_test(function writeFromReturnsPartialCountOnPartialFlush() {
  // Create a pipe for our output stream. This will accept five bytes, and
  // then 'would-block'.
  var outPipe = new Pipe(true, true, 5, 1);

  // Create a reference to the pipe's readable end that can be used
  // from JavaScript.
  var outPipeReadable = new ScriptableInputStream(outPipe.inputStream);

  // Create a buffered stream whose buffer is too large to be flushed
  // entirely to the output pipe.
  var buffered = new BufferedOutputStream(outPipe.outputStream, 7);

  // Create a pipe to be our input stream for the writeFrom call.
  var inPipe = new Pipe(true, true, 15, 1);

  // Write some data to our input pipe, for the rest of the test to consume.
  Assert.equal(inPipe.outputStream.write("0123456789abcde", 15), 15);
  Assert.equal(inPipe.inputStream.available(), 15);

  // Write from the input pipe to the buffered stream. The buffered stream
  // will fill its seven-byte buffer; and then the flush will only succeed
  // in writing five bytes to the output pipe. The writeFrom call should
  // return the number of bytes it consumed from inputStream.
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 11), 7);
  Assert.equal(outPipe.inputStream.available(), 5);
  Assert.equal(inPipe.inputStream.available(), 8);

  // The partially-successful Flush should have created five bytes of
  // available space in the buffered stream's buffer, so we should be able
  // to write five bytes to it without blocking.
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 5), 5);
  Assert.equal(outPipe.inputStream.available(), 5);
  Assert.equal(inPipe.inputStream.available(), 3);

  // Attempting to write any more data should would-block.
  do_check_throws_nsIException(
    () => buffered.writeFrom(inPipe.inputStream, 1),
    "NS_BASE_STREAM_WOULD_BLOCK"
  );

  // No data should have been consumed from the pipe.
  Assert.equal(inPipe.inputStream.available(), 3);

  // Push the rest of the data through, checking that it all came through.
  Assert.equal(outPipeReadable.available(), 5);
  Assert.equal(outPipeReadable.read(5), "01234");
  // Flush returns NS_ERROR_FAILURE if it can't transfer the full amount.
  do_check_throws_nsIException(() => buffered.flush(), "NS_ERROR_FAILURE");
  Assert.equal(outPipeReadable.available(), 5);
  Assert.equal(outPipeReadable.read(5), "56789");
  buffered.flush();
  Assert.equal(outPipeReadable.available(), 2);
  Assert.equal(outPipeReadable.read(2), "ab");
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 3), 3);
  buffered.flush();
  Assert.equal(outPipeReadable.available(), 3);
  Assert.equal(outPipeReadable.read(3), "cde");

  run_next_test();
});

// A writeFrom to a buffered stream should return a partial count if any
// data is written, when the last Flush call blocks.
add_test(function writeFromReturnsPartialCountOnBlock() {
  // Create a pipe for our output stream. This will accept five bytes, and
  // then 'would-block'.
  var outPipe = new Pipe(true, true, 5, 1);

  // Create a reference to the pipe's readable end that can be used
  // from JavaScript.
  var outPipeReadable = new ScriptableInputStream(outPipe.inputStream);

  // Create a buffered stream whose buffer is too large to be flushed
  // entirely to the output pipe.
  var buffered = new BufferedOutputStream(outPipe.outputStream, 7);

  // Create a pipe to be our input stream for the writeFrom call.
  var inPipe = new Pipe(true, true, 15, 1);

  // Write some data to our input pipe, for the rest of the test to consume.
  Assert.equal(inPipe.outputStream.write("0123456789abcde", 15), 15);
  Assert.equal(inPipe.inputStream.available(), 15);

  // Write enough from the input pipe to the buffered stream to fill the
  // output pipe's buffer, and then flush it. Nothing should block or fail,
  // but the output pipe should now be full.
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 5), 5);
  buffered.flush();
  Assert.equal(outPipe.inputStream.available(), 5);
  Assert.equal(inPipe.inputStream.available(), 10);

  // Now try to write more from the input pipe than the buffered stream's
  // buffer can hold. It will attempt to flush, but the output pipe will
  // would-block without accepting any data. writeFrom should return the
  // correct partial count.
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 10), 7);
  Assert.equal(outPipe.inputStream.available(), 5);
  Assert.equal(inPipe.inputStream.available(), 3);

  // Attempting to write any more data should would-block.
  do_check_throws_nsIException(
    () => buffered.writeFrom(inPipe.inputStream, 3),
    "NS_BASE_STREAM_WOULD_BLOCK"
  );

  // No data should have been consumed from the pipe.
  Assert.equal(inPipe.inputStream.available(), 3);

  // Push the rest of the data through, checking that it all came through.
  Assert.equal(outPipeReadable.available(), 5);
  Assert.equal(outPipeReadable.read(5), "01234");
  // Flush returns NS_ERROR_FAILURE if it can't transfer the full amount.
  do_check_throws_nsIException(() => buffered.flush(), "NS_ERROR_FAILURE");
  Assert.equal(outPipeReadable.available(), 5);
  Assert.equal(outPipeReadable.read(5), "56789");
  Assert.equal(buffered.writeFrom(inPipe.inputStream, 3), 3);
  buffered.flush();
  Assert.equal(outPipeReadable.available(), 5);
  Assert.equal(outPipeReadable.read(5), "abcde");

  run_next_test();
});
