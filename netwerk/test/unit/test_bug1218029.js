/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var tests = [
  {data: '', chunks: [], status: Cr.NS_OK, consume: [],
   dataChunks: ['']},
  {data: 'TWO-PARTS', chunks: [4, 5], status: Cr.NS_OK, consume: [4, 5],
   dataChunks: ['TWO-', 'PARTS', '']},
  {data: 'TWO-PARTS', chunks: [4, 5], status: Cr.NS_OK, consume: [0, 0],
   dataChunks: ['TWO-', 'TWO-PARTS', 'TWO-PARTS']},
  {data: '3-PARTS', chunks: [1, 1, 5], status: Cr.NS_OK, consume: [0, 2, 5],
   dataChunks: ['3', '3-', 'PARTS', '']},
  {data: 'ALL-AT-ONCE', chunks: [11], status: Cr.NS_OK, consume: [0],
   dataChunks: ['ALL-AT-ONCE', 'ALL-AT-ONCE']},
  {data: 'ALL-AT-ONCE', chunks: [11], status: Cr.NS_OK, consume: [11],
   dataChunks: ['ALL-AT-ONCE', '']},
  {data: 'ERROR', chunks: [1], status: Cr.NS_ERROR_OUT_OF_MEMORY, consume: [0],
   dataChunks: ['E', 'E']}
];

/**
 * @typedef TestData
 * @property {string} data - data for the test.
 * @property {Array} chunks - lengths of the chunks that are incrementally sent
 *   to the loader.
 * @property {number} status - final status sent on onStopRequest.
 * @property {Array} consume - lengths of consumed data that is reported at
 *   the onIncrementalData callback.
 * @property {Array} dataChunks - data chunks that are reported at the
 *   onIncrementalData and onStreamComplete callbacks.
 */

function execute_test(test) {
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(Ci.nsIStringInputStream);
  stream.data = test.data;

  let channel = {
    contentLength: -1,
    QueryInterface: ChromeUtils.generateQI([Ci.nsIChannel])
  };

  let chunkIndex = 0;

  let observer = {
    onStreamComplete: function(loader, context, status, length, data) {
      equal(chunkIndex, test.dataChunks.length - 1);
      var expectedChunk = test.dataChunks[chunkIndex];
      equal(length, expectedChunk.length);
      equal(String.fromCharCode.apply(null, data), expectedChunk);

      equal(status, test.status);
    },
    onIncrementalData: function (loader, context, length, data, consumed) {
      ok(chunkIndex < test.dataChunks.length - 1);
      var expectedChunk = test.dataChunks[chunkIndex];
      equal(length, expectedChunk.length);
      equal(String.fromCharCode.apply(null, data), expectedChunk);

      consumed.value = test.consume[chunkIndex];
      chunkIndex++;
    },
    QueryInterface:
      ChromeUtils.generateQI([Ci.nsIIncrementalStreamLoaderObserver])
  };

  let listener = Cc["@mozilla.org/network/incremental-stream-loader;1"]
                 .createInstance(Ci.nsIIncrementalStreamLoader);
  listener.init(observer);

  listener.onStartRequest(channel, null);
  var offset = 0;
  test.chunks.forEach(function (chunkLength) {
    listener.onDataAvailable(channel, null, stream, offset, chunkLength);
    offset += chunkLength;
  });
  listener.onStopRequest(channel, null, test.status);
}

function run_test() {
  tests.forEach(execute_test);
}
