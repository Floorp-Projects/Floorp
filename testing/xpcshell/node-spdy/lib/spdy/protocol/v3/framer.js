var framer = exports;

var spdy = require('../../../spdy'),
    Buffer = require('buffer').Buffer,
    protocol = require('./');

//
// ### function Framer (deflate, inflate)
// #### @deflate {zlib.Deflate} Deflate stream
// #### @inflate {zlib.Inflate} Inflate stream
// Framer constructor
//
function Framer(deflate, inflate) {
  this.version = 3;
  this.deflate = deflate;
  this.inflate = inflate;
}
exports.Framer = Framer;

//
// ### function execute (header, body, callback)
// #### @header {Object} Frame headers
// #### @body {Buffer} Frame's body
// #### @callback {Function} Continuation callback
// Parse frame (decompress data and create streams)
//
Framer.prototype.execute = function execute(header, body, callback) {
  // SYN_STREAM or SYN_REPLY
  if (header.type === 0x01 || header.type === 0x02) {
    var frame = protocol.parseSynHead(header.type, header.flags, body);

    body = body.slice(frame._offset);

    this.inflate(body, function(err, chunks, length) {
      if (err) return callback(err);

      var pairs = new Buffer(length);
      for (var i = 0, offset = 0; i < chunks.length; i++) {
        chunks[i].copy(pairs, offset);
        offset += chunks[i].length;
      }

      frame.headers = protocol.parseHeaders(pairs);
      frame.url = frame.headers.path || '';

      callback(null, frame);
    });
  // RST_STREAM
  } else if (header.type === 0x03) {
    callback(null, protocol.parseRst(body));
  // SETTINGS
  } else if (header.type === 0x04) {
    callback(null, protocol.parseSettings(body));
  } else if (header.type === 0x05) {
    callback(null, { type: 'NOOP' });
  // PING
  } else if (header.type === 0x06) {
    callback(null, { type: 'PING', pingId: body });
  // GOAWAY
  } else if (header.type === 0x07) {
    callback(null, protocol.parseGoaway(body));
  } else if (header.type === 0x09) {
    callback(null, protocol.parseWindowUpdate(body));
  } else {
    callback(null, { type: 'unknown: ' + header.type, body: body });
  }
};

//
// internal, converts object into spdy dictionary
//
function headersToDict(headers, preprocess) {
  function stringify(value) {
    if (value !== undefined) {
      if (Array.isArray(value)) {
        return value.join('\x00');
      } else if (typeof value === 'string') {
        return value;
      } else {
        return value.toString();
      }
    } else {
      return '';
    }
  }

  // Lower case of all headers keys
  var loweredHeaders = {};
  Object.keys(headers || {}).map(function(key) {
    loweredHeaders[key.toLowerCase()] = headers[key];
  });

  // Allow outer code to add custom headers or remove something
  if (preprocess) preprocess(loweredHeaders);

  // Transform object into kv pairs
  var len = 4,
      pairs = Object.keys(loweredHeaders).filter(function(key) {
        var lkey = key.toLowerCase();
        return lkey !== 'connection' && lkey !== 'keep-alive' &&
               lkey !== 'proxy-connection' && lkey !== 'transfer-encoding';
      }).map(function(key) {
        var klen = Buffer.byteLength(key),
            value = stringify(loweredHeaders[key]),
            vlen = Buffer.byteLength(value);

        len += 8 + klen + vlen;
        return [klen, key, vlen, value];
      }),
      result = new Buffer(len);

  result.writeUInt32BE(pairs.length, 0, true);

  var offset = 4;
  pairs.forEach(function(pair) {
    // Write key length
    result.writeUInt32BE(pair[0], offset, true);
    // Write key
    result.write(pair[1], offset + 4);

    offset += pair[0] + 4;

    // Write value length
    result.writeUInt32BE(pair[2], offset, true);
    // Write value
    result.write(pair[3], offset + 4);

    offset += pair[2] + 4;
  });

  return result;
};

Framer.prototype._synFrame = function _synFrame(type, id, assoc, priority, dict,
                                                callback) {
  // Compress headers
  this.deflate(dict, function (err, chunks, size) {
    if (err) return callback(err);

    var offset = type === 'SYN_STREAM' ? 18 : 12,
        total = (type === 'SYN_STREAM' ? 10 : 4) + size,
        frame = new Buffer(offset + size);;

    frame.writeUInt16BE(0x8003, 0, true); // Control + Version
    frame.writeUInt16BE(type === 'SYN_STREAM' ? 1 : 2, 2, true); // type
    frame.writeUInt32BE(total & 0x00ffffff, 4, true); // No flag support
    frame.writeUInt32BE(id & 0x7fffffff, 8, true); // Stream-ID

    if (type === 'SYN_STREAM') {
      frame[4] = 2;
      frame.writeUInt32BE(assoc & 0x7fffffff, 12, true); // Stream-ID
    }

    frame.writeUInt8(priority & 0x7, 16, true); // Priority

    for (var i = 0; i < chunks.length; i++) {
      chunks[i].copy(frame, offset);
      offset += chunks[i].length;
    }

    callback(null, frame);
  });
};

//
// ### function replyFrame (id, code, reason, headers, callback)
// #### @id {Number} Stream ID
// #### @code {Number} HTTP Status Code
// #### @reason {String} (optional)
// #### @headers {Object|Array} (optional) HTTP headers
// #### @callback {Function} Continuation function
// Sends SYN_REPLY frame
//
Framer.prototype.replyFrame = function replyFrame(id, code, reason, headers,
                                                  callback) {
  var dict = headersToDict(headers, function(headers) {
    headers[':status'] = code + ' ' + reason;
    headers[':version'] = 'HTTP/1.1';
  });

  this._synFrame('SYN_REPLY', id, null, 0, dict, callback);
};

//
// ### function streamFrame (id, assoc, headers, callback)
// #### @id {Number} stream id
// #### @assoc {Number} associated stream id
// #### @meta {Object} meta headers ( method, scheme, url, version )
// #### @headers {Object} stream headers
// #### @callback {Function} continuation callback
// Create SYN_STREAM frame
// (needed for server push and testing)
//
Framer.prototype.streamFrame = function streamFrame(id, assoc, meta, headers,
                                                    callback) {
  var dict = headersToDict(headers, function(headers) {
    headers[':status'] = 200;
    headers[':version'] = meta.version || 'HTTP/1.1';
    headers[':path'] = meta.path;
    headers[':scheme'] = meta.scheme || 'https';
    headers[':host'] = meta.host;
  });

  this._synFrame('SYN_STREAM', id, assoc, meta.priority, dict, callback);
};

//
// ### function dataFrame (id, fin, data)
// #### @id {Number} Stream id
// #### @fin {Bool} Is this data frame last frame
// #### @data {Buffer} Response data
// Sends DATA frame
//
Framer.prototype.dataFrame = function dataFrame(id, fin, data) {
  if (!fin && !data.length) return [];

  var frame = new Buffer(8 + data.length);

  frame.writeUInt32BE(id & 0x7fffffff, 0, true);
  frame.writeUInt32BE(data.length & 0x00ffffff, 4, true);
  frame.writeUInt8(fin ? 0x01 : 0x0, 4, true);

  if (data.length) data.copy(frame, 8);

  return frame;
};

//
// ### function pingFrame (id)
// #### @id {Buffer} Ping ID
// Sends PING frame
//
Framer.prototype.pingFrame = function pingFrame(id) {
  var header = new Buffer(12);

  header.writeUInt32BE(0x80030006, 0, true); // Version and type
  header.writeUInt32BE(0x00000004, 4, true); // Length
  id.copy(header, 8, 0, 4); // ID

  return header;
};

//
// ### function rstFrame (id, code)
// #### @id {Number} Stream ID
// #### @code {NUmber} RST Code
// Sends PING frame
//
Framer.prototype.rstFrame = function rstFrame(id, code) {
  var header;

  if (!(header = Framer.rstCache[code])) {
    header = new Buffer(16);

    header.writeUInt32BE(0x80030003, 0, true); // Version and type
    header.writeUInt32BE(0x00000008, 4, true); // Length
    header.writeUInt32BE(id & 0x7fffffff, 8, true); // Stream ID
    header.writeUInt32BE(code, 12, true); // Status Code

    Framer.rstCache[code] = header;
  }

  return header;
};
Framer.rstCache = {};

//
// ### function settingsFrame (options)
// #### @options {Object} settings frame options
// Sends SETTINGS frame with MAX_CONCURRENT_STREAMS and initial window
//
Framer.prototype.settingsFrame = function settingsFrame(options) {
  var settings,
      key = options.maxStreams + ':' + options.windowSize;

  if (!(settings = Framer.settingsCache[key])) {
    settings = new Buffer(28);

    settings.writeUInt32BE(0x80030004, 0, true); // Version and type
    settings.writeUInt32BE((4 + 8 * 2) & 0x00FFFFFF, 4, true); // length
    settings.writeUInt32BE(0x00000002, 8, true); // Count of entries

    settings.writeUInt32BE(0x01000004, 12, true); // Entry ID and Persist flag
    settings.writeUInt32BE(options.maxStreams & 0x7fffffff, 16, true);

    settings.writeUInt32BE(0x01000007, 20, true); // Entry ID and Persist flag
    settings.writeUInt32BE(options.windowSize & 0x7fffffff, 24, true);

    Framer.settingsCache[key] = settings;
  }

  return settings;
};
Framer.settingsCache = {};

//
// ### function windowSizeFrame (size)
// #### @size {Number} data transfer window size
// Sends SETTINGS frame with window size
//
Framer.prototype.windowSizeFrame = function windowSizeFrame(size) {
  var settings;

  if (!(settings = Framer.windowSizeCache[size])) {
    settings = new Buffer(20);

    settings.writeUInt32BE(0x80030004, 0, true); // Version and type
    settings.writeUInt32BE((4 + 8) & 0x00FFFFFF, 4, true); // length
    settings.writeUInt32BE(0x00000001, 8, true); // Count of entries

    settings.writeUInt32BE(0x01000007, 12, true); // Entry ID and Persist flag
    settings.writeUInt32BE(size & 0x7fffffff, 16, true); // Window Size (KB)

    Framer.windowSizeCache[size] = settings;
  }

  return settings;
};
Framer.windowSizeCache = {};

//
// ### function windowUpdateFrame (id)
// #### @id {Buffer} WindowUpdate ID
// Sends WINDOW_UPDATE frame
//
Framer.prototype.windowUpdateFrame = function windowUpdateFrame(id, delta) {
  var header = new Buffer(16);

  header.writeUInt32BE(0x80030009, 0, true); // Version and type
  header.writeUInt32BE(0x00000008, 4, true); // Length
  header.writeUInt32BE(id & 0x7fffffff, 8, true); // ID
  header.writeUInt32BE(delta & 0x7fffffff, 12, true); // delta

  return header;
};
