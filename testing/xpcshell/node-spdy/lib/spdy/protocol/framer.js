var spdy = require('../../spdy');
var util = require('util');
var Buffer = require('buffer').Buffer;
var EventEmitter = require('events').EventEmitter;
var constants = require('./').constants;

function Framer() {
  EventEmitter.call(this);

  this.version = null;
  this.deflate = null;
  this.inflate = null;
  this.debug = false;
};
util.inherits(Framer, EventEmitter);
module.exports = Framer;

Framer.create = function create(version, deflate, inflate) {
  return new Framer(version, deflate, inflate);
};

//
// ### function setCompression (deflate, inflate)
// #### @deflate {Deflate}
// #### @inflate {Inflate}
// Set framer compression
//
Framer.prototype.setCompression = function setCompresion(deflate, inflate) {
  this.deflate = spdy.utils.zwrap(deflate);
  this.inflate = spdy.utils.zwrap(inflate);
};


//
// ### function setCompression (deflate, inflate)
// #### @deflate {Deflate}
// #### @inflate {Inflate}
// Set framer compression
//
Framer.prototype.setVersion = function setVersion(version) {
  this.version = version;
  this.emit('version');
};

//
// internal, converts object into spdy dictionary
//
Framer.prototype.headersToDict = function headersToDict(headers, preprocess) {
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
  if (preprocess)
    preprocess(loweredHeaders);

  // Transform object into kv pairs
  var size = this.version === 2 ? 2 : 4,
      len = size,
      pairs = Object.keys(loweredHeaders).filter(function(key) {
        var lkey = key.toLowerCase();
        return lkey !== 'connection' && lkey !== 'keep-alive' &&
               lkey !== 'proxy-connection' && lkey !== 'transfer-encoding';
      }).map(function(key) {
        var klen = Buffer.byteLength(key),
            value = stringify(loweredHeaders[key]),
            vlen = Buffer.byteLength(value);

        len += size * 2 + klen + vlen;
        return [klen, key, vlen, value];
      }),
      result = new Buffer(len);

  if (this.version === 2)
    result.writeUInt16BE(pairs.length, 0, true);
  else
    result.writeUInt32BE(pairs.length, 0, true);

  var offset = size;
  pairs.forEach(function(pair) {
    // Write key length
    if (this.version === 2)
      result.writeUInt16BE(pair[0], offset, true);
    else
      result.writeUInt32BE(pair[0], offset, true);
    // Write key
    result.write(pair[1], offset + size);

    offset += pair[0] + size;

    // Write value length
    if (this.version === 2)
      result.writeUInt16BE(pair[2], offset, true);
    else
      result.writeUInt32BE(pair[2], offset, true);
    // Write value
    result.write(pair[3], offset + size);

    offset += pair[2] + size;
  }, this);

  return result;
};

Framer.prototype._synFrame = function _synFrame(type,
                                                id,
                                                assoc,
                                                priority,
                                                dict,
                                                callback) {
  var self = this;

  // Compress headers
  this.deflate(dict, function (err, chunks, size) {
    if (err)
      return callback(err);

    var offset = type === 'SYN_STREAM' ? 18 : self.version === 2 ? 14 : 12,
        total = offset - 8 + size,
        frame = new Buffer(offset + size);

    // Control + Version
    frame.writeUInt16BE(0x8000 | self.version, 0, true);
    // Type
    frame.writeUInt16BE(type === 'SYN_STREAM' ? 1 : 2, 2, true);
    // Size
    frame.writeUInt32BE(total & 0x00ffffff, 4, true);
    // Stream ID
    frame.writeUInt32BE(id & 0x7fffffff, 8, true);

    if (type === 'SYN_STREAM') {
      // Unidirectional
      if (assoc !== 0)
        frame[4] = 2;

      // Associated Stream-ID
      frame.writeUInt32BE(assoc & 0x7fffffff, 12, true);

      // Priority
      var priorityValue;
      if (self.version === 2)
        priorityValue = Math.max(Math.min(priority, 3), 0) << 6;
      else
        priorityValue = Math.max(Math.min(priority, 7), 0) << 5;
      frame.writeUInt8(priorityValue, 16, true);
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
Framer.prototype.replyFrame = function replyFrame(id,
                                                  code,
                                                  reason,
                                                  headers,
                                                  callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.replyFrame(id, code, reason, headers, callback);
    });
  }

  var self = this;
  var dict = this.headersToDict(headers, function(headers) {
    if (self.version === 2) {
      headers.status = code + ' ' + reason;
      headers.version = 'HTTP/1.1';
    } else {
      headers[':status'] = code + ' ' + reason;
      headers[':version'] = 'HTTP/1.1';
    }
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
Framer.prototype.streamFrame = function streamFrame(id,
                                                    assoc,
                                                    meta,
                                                    headers,
                                                    callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.streamFrame(id, assoc, meta, headers, callback);
    });
  }

  var self = this;
  var dict = this.headersToDict(headers, function(headers) {
    if (self.version === 2) {
      if (meta.status)
        headers.status = meta.status;
      headers.version = meta.version || 'HTTP/1.1';
      headers.url = meta.url;
      if (meta.method)
        headers.method = meta.method;
    } else {
      if (meta.status)
        headers[':status'] = meta.status;
      headers[':version'] = meta.version || 'HTTP/1.1';
      headers[':path'] = meta.path || meta.url;
      headers[':scheme'] = meta.scheme || 'https';
      headers[':host'] = meta.host;
      if (meta.method)
        headers[':method'] = meta.method;
    }
  });

  this._synFrame('SYN_STREAM', id, assoc, meta.priority, dict, callback);
};

//
// ### function headersFrame (id, headers, callback)
// #### @id {Number} Stream id
// #### @headers {Object} Headers
// #### @callback {Function}
// Sends HEADERS frame
//
Framer.prototype.headersFrame = function headersFrame(id, headers, callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.headersFrame(id, headers, callback);
    });
  }

  var self = this;
  var dict = this.headersToDict(headers, function(headers) {});

  this.deflate(dict, function (err, chunks, size) {
    if (err)
      return callback(err);

    var offset = self.version === 2 ? 14 : 12,
        total = offset - 8 + size,
        frame = new Buffer(offset + size);

    // Control + Version
    frame.writeUInt16BE(0x8000 | self.version, 0, true);
    // Type
    frame.writeUInt16BE(8, 2, true);
    // Length
    frame.writeUInt32BE(total & 0x00ffffff, 4, true);
    // Stream ID
    frame.writeUInt32BE(id & 0x7fffffff, 8, true);

    // Copy chunks
    for (var i = 0; i < chunks.length; i++) {
      chunks[i].copy(frame, offset);
      offset += chunks[i].length;
    }

    callback(null, frame);
  });
};

//
// ### function dataFrame (id, fin, data, callback)
// #### @id {Number} Stream id
// #### @fin {Bool} Is this data frame last frame
// #### @data {Buffer} Response data
// #### @callback {Function}
// Sends DATA frame
//
Framer.prototype.dataFrame = function dataFrame(id, fin, data, callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.dataFrame(id, fin, data, callback);
    });
  }

  if (!fin && !data.length)
    return callback(null, []);

  var frame = new Buffer(8 + data.length);

  frame.writeUInt32BE(id & 0x7fffffff, 0, true);
  frame.writeUInt32BE(data.length & 0x00ffffff, 4, true);
  frame.writeUInt8(fin ? 0x01 : 0x0, 4, true);

  if (data.length)
    data.copy(frame, 8);

  return callback(null, frame);
};

//
// ### function pingFrame (id)
// #### @id {Number} Ping ID
// Sends PING frame
//
Framer.prototype.pingFrame = function pingFrame(id, callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.pingFrame(id, callback);
    });
  }

  var header = new Buffer(12);

  // Version and type
  header.writeUInt32BE(0x80000006 | (this.version << 16), 0, true);
  // Length
  header.writeUInt32BE(0x00000004, 4, true);
  // ID
  header.writeUInt32BE(id, 8, true);

  return callback(null, header);
};

//
// ### function rstFrame (id, code, extra, callback)
// #### @id {Number} Stream ID
// #### @code {Number} RST Code
// #### @extra {String} Extra debugging info
// #### @callback {Function}
// Sends PING frame
//
Framer.prototype.rstFrame = function rstFrame(id, code, extra, callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.rstFrame(id, code, extra, callback);
    });
  }

  var header = new Buffer(16 +
                          (this.debug ? Buffer.byteLength(extra || '') : 0));

  // Version and type
  header.writeUInt32BE(0x80000003 | (this.version << 16), 0, true);
  // Length
  header.writeUInt32BE(0x00000008, 4, true);
  // Stream ID
  header.writeUInt32BE(id & 0x7fffffff, 8, true);
  // Status Code
  header.writeUInt32BE(code, 12, true);

  // Extra debugging information
  if (this.debug && extra)
    header.write(extra, 16);

  return callback(null, header);
};

//
// ### function settingsFrame (options, callback)
// #### @options {Object} settings frame options
// #### @callback {Function}
// Sends SETTINGS frame with MAX_CONCURRENT_STREAMS and initial window
//
Framer.prototype.settingsFrame = function settingsFrame(options, callback) {
  if (!this.version) {
    return this.on('version', function() {
      this.settingsFrame(options, callback);
    });
  }

  var settings,
      key = this.version === 2 ? '2/' + options.maxStreams :
                                 '3/' + options.maxStreams + ':' +
                                     options.windowSize;

  if (!(settings = Framer.settingsCache[key])) {
    var params = [];
    if (isFinite(options.maxStreams)) {
      params.push({
        key: constants.settings.SETTINGS_MAX_CONCURRENT_STREAMS,
        value: options.maxStreams
      });
    }
    if (this.version > 2) {
      params.push({
        key: constants.settings.SETTINGS_INITIAL_WINDOW_SIZE,
        value: options.windowSize
      });
    }

    settings = new Buffer(12 + 8 * params.length);

    // Version and type
    settings.writeUInt32BE(0x80000004 | (this.version << 16), 0, true);
    // Length
    settings.writeUInt32BE((4 + 8 * params.length) & 0x00FFFFFF, 4, true);
    // Count of entries
    settings.writeUInt32BE(params.length, 8, true);

    var offset = 12;
    params.forEach(function(param) {
      var flag = constants.settings.FLAG_SETTINGS_PERSIST_VALUE << 24;

      if (this.version === 2)
        settings.writeUInt32LE(flag | param.key, offset, true);
      else
        settings.writeUInt32BE(flag | param.key, offset, true);
      offset += 4;
      settings.writeUInt32BE(param.value & 0x7fffffff, offset, true);
      offset += 4;
    }, this);

    Framer.settingsCache[key] = settings;
  }

  return callback(null, settings);
};
Framer.settingsCache = {};

//
// ### function windowUpdateFrame (id)
// #### @id {Buffer} WindowUpdate ID
// Sends WINDOW_UPDATE frame
//
Framer.prototype.windowUpdateFrame = function windowUpdateFrame(id, delta, cb) {
  if (!this.version) {
    return this.on('version', function() {
      this.windowUpdateFrame(id, delta, cb);
    });
  }

  var header = new Buffer(16);

  // Version and type
  header.writeUInt32BE(0x80000009 | (this.version << 16), 0, true);
  // Length
  header.writeUInt32BE(0x00000008, 4, true);
  // ID
  header.writeUInt32BE(id & 0x7fffffff, 8, true);
  // Delta
  if (delta > 0)
    header.writeUInt32BE(delta & 0x7fffffff, 12, true);
  else
    header.writeUInt32BE(delta, 12, true);

  return cb(null, header);
};

Framer.prototype.goawayFrame = function goawayFrame(lastId, status, cb) {
  if (!this.version) {
    return this.on('version', function() {
      this.goawayFrame(lastId, status, cb);
    });
  }

  var header = new Buffer(16);

  // Version and type
  header.writeUInt32BE(0x80000007 | (this.version << 16), 0, true);
  // Length
  header.writeUInt32BE(0x00000008, 4, true);
  // Last-good-stream-ID
  header.writeUInt32BE(lastId & 0x7fffffff, 8, true);
  // Status
  header.writeUInt32BE(status, 12, true);

  return cb(null, header);
};
