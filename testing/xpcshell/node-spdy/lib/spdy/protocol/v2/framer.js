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
      var pairs = new Buffer(length);
      for (var i = 0, offset = 0; i < chunks.length; i++) {
        chunks[i].copy(pairs, offset);
        offset += chunks[i].length;
      }

      frame.headers = protocol.parseHeaders(pairs);

      callback(null, frame);
    });
  // RST_STREAM
  } else if (header.type === 0x03) {
    callback(null, protocol.parseRst(body));
  // SETTINGS
  } else if (header.type === 0x04) {
    callback(null, { type: 'SETTINGS' });
  } else if (header.type === 0x05) {
    callback(null, { type: 'NOOP' });
  // PING
  } else if (header.type === 0x06) {
    callback(null, { type: 'PING', pingId: body });
  // GOAWAY
  } else if (header.type === 0x07) {
    callback(null, protocol.parseGoaway(body));
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
  var len = 2,
      pairs = Object.keys(loweredHeaders).filter(function(key) {
        var lkey = key.toLowerCase();
        return lkey !== 'connection' && lkey !== 'keep-alive' &&
               lkey !== 'proxy-connection' && lkey !== 'transfer-encoding';
      }).map(function(key) {
        var klen = Buffer.byteLength(key),
            value = stringify(loweredHeaders[key]),
            vlen = Buffer.byteLength(value);

        len += 4 + klen + vlen;
        return [klen, key, vlen, value];
      }),
      result = new Buffer(len);

  result.writeUInt16BE(pairs.length, 0, true);

  var offset = 2;
  pairs.forEach(function(pair) {
    // Write key length
    result.writeUInt16BE(pair[0], offset, true);
    // Write key
    result.write(pair[1], offset + 2);

    offset += pair[0] + 2;

    // Write value length
    result.writeUInt16BE(pair[2], offset, true);
    // Write value
    result.write(pair[3], offset + 2);

    offset += pair[2] + 2;
  });

  return result;
};

Framer.prototype._synFrame = function _synFrame(type, id, assoc, dict,
                                                callback) {
  // Compress headers
  this.deflate(dict, function (err, chunks, size) {
    if (err) return callback(err);

    var offset = type === 'SYN_STREAM' ? 18 : 14,
        total = (type === 'SYN_STREAM' ? 10 : 6) + size,
        frame = new Buffer(offset + size);;

    frame.writeUInt16BE(0x8002, 0, true); // Control + Version
    frame.writeUInt16BE(type === 'SYN_STREAM' ? 1 : 2, 2, true); // type
    frame.writeUInt32BE(total & 0x00ffffff, 4, true); // No flag support
    frame.writeUInt32BE(id & 0x7fffffff, 8, true); // Stream-ID

    if (type === 'SYN_STREAM') {
      frame[4] = 2;
      frame.writeUInt32BE(assoc & 0x7fffffff, 12, true); // Stream-ID
    }

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
        headers.status = code + ' ' + reason;
        headers.version = 'HTTP/1.1';
      });

  this._synFrame('SYN_REPLY', id, null, dict, callback);
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
    headers.status = 200;
    headers.version = 'HTTP/1.1';
    headers.url = meta.url;
  });

  this._synFrame('SYN_STREAM', id, assoc, dict, callback);
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

  header.writeUInt32BE(0x80020006, 0, true); // Version and type
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

    header.writeUInt32BE(0x80020003, 0, true); // Version and type
    header.writeUInt32BE(0x00000008, 4, true); // Length
    header.writeUInt32BE(id & 0x7fffffff, 8, true); // Stream ID
    header.writeUInt32BE(code, 12, true); // Status Code

    Framer.rstCache[code] = header;
  }

  return header;
};
Framer.rstCache = {};

//
// ### function maxStreamsFrame (count)
// #### @count {Number} Max Concurrent Streams count
// Sends SETTINGS frame with MAX_CONCURRENT_STREAMS
//
Framer.prototype.maxStreamsFrame = function maxStreamsFrame(count) {
  var settings;

  if (!(settings = Framer.settingsCache[count])) {
    settings = new Buffer(20);

    settings.writeUInt32BE(0x80020004, 0, true); // Version and type
    settings.writeUInt32BE(0x0000000C, 4, true); // length
    settings.writeUInt32BE(0x00000001, 8, true); // Count of entries
    settings.writeUInt32LE(0x01000004, 12, true); // Entry ID and Persist flag
    settings.writeUInt32BE(count, 16, true); // 100 Streams

    Framer.settingsCache[count] = settings;
  }

  return settings;
};
Framer.settingsCache = {};
