var spdy = require('../spdy'),
    utils = exports;

var zlib = require('zlib'),
    Buffer = require('buffer').Buffer;

//
// ### function createDeflate ()
// Creates deflate stream with SPDY dictionary
//
utils.createDeflate = function createDeflate(version) {
  var deflate = zlib.createDeflate({
    dictionary: spdy.protocol[version].dictionary,
    windowBits: 11
  });

  // Define lock information early
  deflate.locked = false;
  deflate.lockBuffer = [];

  return deflate;
};

//
// ### function createInflate ()
// Creates inflate stream with SPDY dictionary
//
utils.createInflate = function createInflate(version) {
  var inflate = zlib.createInflate({
    dictionary: spdy.protocol[version].dictionary,
    windowBits: 15
  });

  // Define lock information early
  inflate.locked = false;
  inflate.lockBuffer = [];

  return inflate;
};

//
// ### function resetZlibStream (stream)
// #### @stream {zlib.Stream} stream
// Resets zlib stream and associated locks
//
utils.resetZlibStream = function resetZlibStream(stream, callback) {
  if (stream.locked) {
    stream.lockBuffer.push(function() {
      resetZlibStream(stream, callback);
    });
    return;
  }

  stream.reset();
  stream.lockBuffer = [];

  callback(null);
};

var delta = 0;
//
// ### function zstream (stream, buffer, callback)
// #### @stream {Deflate|Inflate} One of streams above
// #### @buffer {Buffer} Input data (to compress or to decompress)
// #### @callback {Function} Continuation to callback
// Compress/decompress data and pass it to callback
//
utils.zstream = function zstream(stream, buffer, callback) {
  var flush = stream._flush,
      chunks = [],
      total = 0;

  if (stream.locked) {
    stream.lockBuffer.push(function() {
      zstream(stream, buffer, callback);
    });
    return;
  }
  stream.locked = true;

  function collect(chunk) {
    chunks.push(chunk);
    total += chunk.length;
  }
  stream.on('data', collect);
  stream.write(buffer);

  stream.once('error', function(err) {
    stream.removeAllListeners('data');
    callback(err);
  });

  stream.flush(function() {
    stream.removeAllListeners('data');
    stream.removeAllListeners('error');
    stream._flush = flush;

    callback(null, chunks, total);

    stream.locked = false;
    var deferred = stream.lockBuffer.shift();
    if (deferred) deferred();
  });
};

//
// ### function zwrap (stream)
// #### @stream {zlib.Stream} stream to wrap
// Wraps stream within function to allow simple deflate/inflate
//
utils.zwrap = function zwrap(stream) {
  return function(data, callback) {
    utils.zstream(stream, data, callback);
  };
};
