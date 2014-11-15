var spdy = require('../spdy');
var zlib = require('zlib');
var utils = spdy.utils;
var assert = require('assert');
var util = require('util');
var stream = require('stream');
var Buffer = require('buffer').Buffer;
var constants = spdy.protocol.constants;

if (!/^v(0\.10|0\.8|0\.9)\./.test(process.version))
  var httpCommon = require('_http_common');


//
// ### function Stream (connection, options)
// #### @connection {Connection} SPDY Connection
// #### @options {Object} Stream options
// Abstract stream @constructor
//
function Stream(connection, options) {
  var self = this;

  utils.DuplexStream.call(this);

  this.connection = connection;
  this.socket = connection.socket;
  this.encrypted = connection.encrypted;
  this.associated = null;

  // 0.10 hack
  this._handle = {
    readStop: function() { self._readStop() },
    readStart: function() { self._readStart() }
  };

  var state = {};
  this._spdyState = state;
  state.timeout = connection.timeout;
  state.timeoutTimer = null;
  state.framer = connection._spdyState.framer;
  state.initialized = false;
  state.ending = false;
  state.ended = false;
  state.paused = false;
  state.finishAttached = false;
  state.decompress = options.decompress;
  state.decompressor = null;

  // Store id
  state.id = options.id;
  state.associated = options.associated;
  state.headers = options.headers || {};
  if (connection._spdyState.xForward !== null)
    state.headers['x-forwarded-for'] = connection._spdyState.xForward;

  // Increment counters
  state.isPush = !connection._spdyState.isServer && state.associated;
  connection._spdyState.counters.streamCount++;
  if (state.isPush)
    connection._spdyState.counters.pushCount++;

  // Useful for PUSH streams
  state.scheme = state.headers.scheme;
  state.host = state.headers.host;

  if (state.isPush && state.headers['content-encoding']) {
    var compression = state.headers['content-encoding'];

    // If compression was requested - setup decompressor
    if (compression === 'gzip' || compression === 'deflate')
      this._setupDecompressor(compression);
  }

  // True if inside chunked write
  state.chunkedWrite = false;

  // Store options
  state.options = options;
  state.isClient = !!options.client;
  state.parseRequest = !!options.client;

  // RST_STREAM code if any
  state.rstCode = constants.rst.PROTOCOL_ERROR;
  state.destroyed = false;

  state.closedBy = {
    them: false,
    us: false
  };

  // Store priority
  state.priority = options.priority;

  // How much data can be sent TO client before next WINDOW_UPDATE
  state.sinkSize = connection._spdyState.initialSinkSize;
  state.initialSinkSize = state.sinkSize;

  // When data needs to be send, but window is too small for it - it'll be
  // queued in this buffer
  state.sinkBuffer = [];
  state.sinkDraining = false;

  // How much data can be sent BY client before next WINDOW_UPDATE
  state.windowSize = connection._spdyState.initialWindowSize;
  state.initialWindowSize = state.windowSize;

  this._init();
};
util.inherits(Stream, utils.DuplexStream);
exports.Stream = Stream;

// Parser lookup methods
Stream.prototype._parserHeadersComplete = function parserHeadersComplete() {
  if (this.parser)
    return this.parser.onHeadersComplete || this.parser[1];
};

Stream.prototype._parserBody = function parserBody() {
  if (this.parser)
    return this.parser.onBody || this.parser[2];
};

Stream.prototype._parserMessageComplete = function parserMessageComplete() {
  if (this.parser)
    return this.parser.onMessageComplete || this.parser[3];
};

//
// ### function init ()
// Initialize stream
//
Stream.prototype._init = function init() {
  var state = this._spdyState;

  this.setTimeout();
  this.ondata = this.onend = null;

  if (utils.isLegacy)
    this.readable = this.writable = true;

  // Call .onend()
  this.once('end', function() {
    var self = this;
    utils.nextTick(function() {
      var onHeadersComplete = self._parserHeadersComplete();
      if (!onHeadersComplete && self.onend)
        self.onend();
    });
  });

  // Handle half-close
  this.once('finish', function onfinish() {
    if (state.chunkedWrite)
      return this.once('_chunkDone', onfinish);

    var self = this;
    this._writeData(true, [], function() {
      state.closedBy.us = true;
      if (state.sinkBuffer.length !== 0)
        return;
      if (utils.isLegacy)
        self.emit('full-finish');
      self._handleClose();
    });
  });

  if (state.isClient) {
    var httpMessage;
    Object.defineProperty(this, '_httpMessage', {
      set: function(val) {
        if (val)
          this._attachToRequest(val);
        httpMessage = val;
      },
      get: function() {
        return httpMessage;
      },
      configurable: true,
      enumerable: true
    });
  }
};

Stream.prototype._readStop = function readStop() {
  this._spdyState.paused = true;
};

Stream.prototype._readStart = function readStart() {
  this._spdyState.paused = false;

  // Send window update if needed
  this._read();
};

if (utils.isLegacy) {
  Stream.prototype.pause = function pause() {
    this._readStop();
  };
  Stream.prototype.resume = function resume() {
    this._readStart();
  };
}

//
// ### function _isGoaway ()
// Returns true if any writes to that stream should be ignored
//
Stream.prototype._isGoaway = function _isGoaway() {
  return this.connection._spdyState.goaway &&
         this._spdyState.id > this.connection._spdyState.goaway;
};

//
// ### function start (url, headers)
// #### @url {String}
// #### @headers {Object}
// Start stream, internal
//
Stream.prototype._start = function start(url, headers) {
  var state = this._spdyState;
  var headerList = [];

  // Create list of headeres
  Object.keys(headers).map(function(key) {
    if (key !== 'method' &&
        key !== 'url' &&
        key !== 'version' &&
        key !== 'scheme' &&
        key !== 'status' &&
        key !== 'path') {
      headerList.push(key, headers[key]);
    }
  });

  var info = {
    url: url,
    headers: headerList,
    versionMajor: 1,
    versionMinor: 1,
    method: httpCommon ? httpCommon.methods.indexOf(headers.method) :
                         headers.method,
    statusCode: false,
    upgrade: headers.method === 'CONNECT'
  };

  // Write data directly to the parser callbacks
  var onHeadersComplete = this._parserHeadersComplete();
  if (onHeadersComplete) {
    onHeadersComplete.call(this.parser, info);

    // onHeadersComplete doesn't handle CONNECT
    if (headers.method === 'CONNECT') {
      // We don't need to handle a lack of listeners here, since there
      //   should always be a listener in server.js
      var req = this.parser.incoming;
      if (this.listeners('data').length) {
        // 0.11+ only (assuming nobody other than the http lib has attached)
        this.removeAllListeners('data');
        this.skipBodyParsing = true;
      }
      this.connection.emit('connect', req, req.socket);
    }
  } else {
    this.emit('headersComplete', info);
  }

  state.initialized = true;
};

//
// ### function attachToRequest (req)
// #### @req {ClientRequest}
// Attach to node.js' response
//
Stream.prototype._attachToRequest = function attachToRequest(res) {
  var self = this;

  res.addTrailers = function addTrailers(headers) {
    self.sendHeaders(headers);
  };

  this.on('headers', function(headers) {
    var req = res.parser.incoming;
    if (req) {
      Object.keys(headers).forEach(function(key) {
        req.trailers[key] = headers[key];
      });
      req.emit('trailers', headers);
    }
  });
};

//
// ### function sendHeaders (headers)
// #### @headers {Object}
//
Stream.prototype.sendHeaders = function sendHeaders(headers) {
  var self = this;
  var state = this._spdyState;

  // Reset timeout
  this.setTimeout();

  var connection = this.connection;
  this._lock(function() {
    state.framer.headersFrame(state.id, headers, function(err, frame) {
      if (err) {
        self._unlock();
        return self.emit('error', err);
      }

      connection.write(frame);
      self._unlock();
    });
  });
};

//
// ### function setTimeout (timeout, callback)
// #### @timeout {Number}
// #### @callback {Function}
//
Stream.prototype.setTimeout = function _setTimeout(timeout, callback) {
  var self = this;
  var state = this._spdyState;

  if (callback)
    this.once('timeout', callback);

  // Keep PUSH's parent alive
  if (this.associated)
    this.associated.setTimeout();

  state.timeout = timeout !== undefined ? timeout : state.timeout;

  if (state.timeoutTimer) {
    clearTimeout(state.timeoutTimer);
    state.timeoutTimer = null;
  }

  if (!state.timeout)
    return;

  state.timeoutTimer = setTimeout(function() {
    self.emit('timeout');
  }, state.timeout);
};

//
// ### function _handleClose ()
// Close stream if it was closed by both server and client
//
Stream.prototype._handleClose = function _handleClose() {
  var state = this._spdyState;
  if (state.closedBy.them && state.closedBy.us)
    this.close();
};

//
// ### function close ()
// Destroys stream
//
Stream.prototype.close = function close() {
  this.destroy();
};

//
// ### function destroy (error)
// #### @error {Error} (optional) error
// Destroys stream
//
Stream.prototype.destroy = function destroy(error) {
  var state = this._spdyState;
  if (state.destroyed)
    return;
  state.destroyed = true;

  // Reset timeout
  this.setTimeout(0);

  // Decrement counters
  this.connection._spdyState.counters.streamCount--;
  if (state.isPush)
    this.connection._spdyState.counters.pushCount--;

  // Just for http.js in v0.10
  this.writable = false;
  this.connection._removeStream(this);

  // If stream is not finished, RST frame should be sent to notify client
  // about sudden stream termination.
  if (error || !state.closedBy.us) {
    if (!state.closedBy.us)
      // CANCEL
      if (state.isClient)
        state.rstCode = constants.rst.CANCEL;
      // REFUSED_STREAM if terminated before 'finish' event
      else
        state.rstCode = constants.rst.REFUSED_STREAM;

    if (state.rstCode) {
      var self = this;
      state.framer.rstFrame(state.id,
                            state.rstCode,
                            null,
                            function(err, frame) {
        if (err)
          return self.emit('error', err);
        var scheduler = self.connection._spdyState.scheduler;

        scheduler.scheduleLast(self, frame);
        scheduler.tick();
      });
    }
  }

  var self = this;
  this._recvEnd(function() {
    if (error)
      self.emit('error', error);

    utils.nextTick(function() {
      self.emit('close', !!error);
    });
  }, true);
};

//
// ### function ping (callback)
// #### @callback {Function}
// Send PING frame and invoke callback once received it back
//
Stream.prototype.ping = function ping(callback) {
  return this.connection.ping(callback);
};

//
// ### function destroySoon ()
//
Stream.prototype.destroySoon = function destroySoon() {
  var self = this;
  var state = this._spdyState;

  // Hack for http.js, when running in client mode
  this.writable = false;

  // Already closed - just ignore
  if (state.closedBy.us)
    return;

  // Reset timeout
  this.setTimeout();
  this.end();
};

//
// ### function drainSink (size)
// #### @size {Number}
// Change sink size
//
Stream.prototype._drainSink = function drainSink(size) {
  var state = this._spdyState;
  var oldBuffer = state.sinkBuffer;

  state.sinkBuffer = [];
  state.sinkSize += size;
  state.sinkDraining = true;

  for (var i = 0; i < oldBuffer.length; i++) {
    var item = oldBuffer[i];

    // Last chunk - allow FIN to be added
    if (i === oldBuffer.length - 1)
      state.sinkDraining = false;
    this._writeData(item.fin, item.buffer, item.cb, item.chunked);
  }

  // Handle half-close
  if (state.sinkBuffer.length === 0 && state.closedBy.us)
    this._handleClose();

  if (utils.isLegacy)
    this.emit('drain');
};

//
// ### function _writeData (fin, buffer, cb, chunked)
// #### @fin {Boolean}
// #### @buffer {Buffer}
// #### @cb {Function} **optional**
// #### @chunked {Boolean} **internal**
// Internal function
//
Stream.prototype._writeData = function _writeData(fin, buffer, cb, chunked) {
  // If client is gone - notify caller about it
  if (!this.connection.socket || !this.connection.socket.writable)
    return false;

  var state = this._spdyState;
  if (!state.framer.version) {
    var self = this;
    state.framer.on('version', function() {
      self._writeData(fin, buffer, cb, chunked);
      if (utils.isLegacy)
        self.emit('drain');
    });
    return false;
  }

  // Already closed
  if (state.closedBy.us) {
    this.emit('error', new Error('Write after end!'));
    return false;
  }

  // If the underlying socket is buffering, put the write into the sinkBuffer
  //   to create backpressure.
  if (this.connection._spdyState.socketBuffering) {
    state.sinkBuffer.push({
      fin: fin,
      buffer: buffer,
      cb: cb,
      chunked: chunked
    });
    return false;
  }

  if (state.framer.version >= 3) {
    // Window was exhausted, queue data
    if (state.sinkSize <= 0 ||
        (state.framer.version >= 3.1 &&
         this.connection._spdyState.sinkSize <= 0)) {
      state.sinkBuffer.push({
        fin: fin,
        buffer: buffer,
        cb: cb,
        chunked: chunked
      });
      return false;
    }
  }
  if (state.chunkedWrite && !chunked) {
    var self = this;
    function attach() {
      self.once('_chunkDone', function() {
        if (state.chunkedWrite)
          return attach();
        self._writeData(fin, buffer, cb, false);
      });
    }
    attach();
    return true;
  }

  // Already ended
  if (state.ended && !chunked)
    return false;

  // Ending, add FIN if not set
  if (!fin && !chunked && state.ending) {
    if (utils.isLegacy)
      fin = this.listeners('_chunkDone').length === 0;
    else
      fin = this._writableState.length === 0;
    fin = fin && false && state.sinkBuffer.length === 0 && !state.sinkDraining;
  }
  if (!chunked && fin)
    state.ended = true;

  var maxChunk = this.connection._spdyState.maxChunk;
  // Slice buffer into parts with size <= `maxChunk`
  if (maxChunk && maxChunk < buffer.length) {
    var preend = buffer.length - maxChunk;
    var chunks = [];
    for (var i = 0; i < preend; i += maxChunk)
      chunks.push(buffer.slice(i, i + maxChunk));

    // Last chunk
    chunks.push(buffer.slice(i));

    var self = this;
    function send(err) {
      function done(err) {
        state.chunkedWrite = false;
        self.emit('_chunkDone');
        if (cb)
          cb(err);
      }

      if (err)
        return done(err);

      var chunk = chunks.shift();
      if (chunks.length === 0) {
        self._writeData(fin, chunk, function(err) {
          // Ensure that `finish` listener will catch this
          done(err);
        }, true);
      } else {
        self._writeData(false, chunk, send, true);
      }
    }

    state.chunkedWrite = true;
    send();
    return true;
  }

  if (state.framer.version >= 3) {
    var len = Math.min(state.sinkSize, buffer.length);
    if (state.framer.version >= 3.1)
      len = Math.min(this.connection._spdyState.sinkSize, len);
    this.connection._spdyState.sinkSize -= len;
    state.sinkSize -= len;

    // Only partial write is possible, queue rest for later
    if (len < buffer.length) {
      state.sinkBuffer.push({
        fin: fin,
        buffer: buffer.slice(len),
        cb: cb,
        chunked: chunked
      });
      buffer = buffer.slice(0, len);
      fin = false;
      cb = null;
    }
  }

  // Reset timeout
  this.setTimeout();

  this._lock(function() {
    var stream = this;

    state.framer.dataFrame(state.id, fin, buffer, function(err, frame) {
      if (err) {
        stream._unlock();
        return stream.emit('error', err);
      }

      var scheduler = stream.connection._spdyState.scheduler;
      scheduler.schedule(stream, frame);
      scheduler.tick(cb);

      stream._unlock();
    });
  });

  return true;
};

//
// ### function parseClientRequest (data, cb)
// #### @data {Buffer|String} Input data
// #### @cb {Function} Continuation to proceed to
// Parse first outbound message in client request
//
Stream.prototype._parseClientRequest = function parseClientRequest(data, cb) {
  var state = this._spdyState;

  state.parseRequest = false;

  var lines = data.toString().split(/\r\n\r\n/);
  var body = data.slice(Buffer.byteLength(lines[0]) + 4);
  lines = lines[0].split(/\r\n/g);
  var status = lines[0].match(/^([a-z]+)\s([^\s]+)\s(.*)$/i);
  var headers = {};

  assert(status !== null);
  var method = status[1].toUpperCase();
  var url = status[2];
  var version = status[3].toUpperCase();
  var host = '';

  // Transform headers and determine host
  lines.slice(1).forEach(function(line) {
    // Last line
    if (!line)
      return;

    // Normal line - `Key: Value`
    var match = line.match(/^(.*?):\s*(.*)$/);
    assert(match !== null);

    var key = match[1].toLowerCase();
    var value = match[2];

    if (key === 'host')
      host = value;
    else if (key !== 'connection')
      headers[key] = value;
  }, this);

  // Disable chunked encoding for all future writes
  assert(this._httpMessage);
  var chunkedEncoding = this._httpMessage.chunkedEncoding;
  this._httpMessage.chunkedEncoding = false;

  // Reset timeout
  this.setTimeout();

  var self = this;
  var connection = this.connection;
  this._lock(function() {
    state.framer.streamFrame(state.id, 0, {
      method: method,
      host: host,
      url: url,
      version: version,
      priority: self.priority
    }, headers, function(err, frame) {
      if (err) {
        self._unlock();
        return self.emit('error', err);
      }
      connection.write(frame);
      self._unlock();
      connection._addStream(self);

      self.emit('_spdyRequest');
      state.initialized = true;
      if (cb)
        cb();
    })
  });

  // Yeah, node.js gave us a body with the request
  if (body) {
    if (chunkedEncoding) {
      var i = 0;
      while (i < body.length) {
        var lenStart = i;

        // Skip length and \r\n
        for (; i + 1 < body.length; i++)
          if (body[i] === 0xd && body[i + 1] === 0xa)
            break;
        if (i === body.length - 1)
          return self.emit('error', new Error('Incorrect chunk length'));

        // Parse length
        var len = parseInt(body.slice(lenStart, i).toString(), 16);

        // Get body chunk
        if (i + 2 + len >= body.length)
          return self.emit('error', new Error('Chunk length OOB'));

        // Ignore empty chunks
        if (len !== 0) {
          var chunk = body.slice(i + 2, i + 2 + len);
          this._write(chunk, null, null);
        }

        // Skip body and '\r\n' after it
        i += 4 + len;
      }
    }
  }

  return true;
};

//
// ### function handleResponse (frame)
// #### @frame {Object} SYN_REPLY frame
// Handle SYN_REPLY
//
Stream.prototype._handleResponse = function handleResponse(frame) {
  var state = this._spdyState;
  assert(state.isClient);

  var headers = frame.headers;
  var headerList = [];
  var compression = null;

  // Create list of headeres
  Object.keys(headers).map(function(key) {
    var val = headers[key];

    if (state.decompress &&
        key === 'content-encoding' &&
        (val === 'gzip' || val === 'deflate'))
      compression = val;
    else if (key !== 'status' && key !== 'version')
      headerList.push(key, headers[key]);
  });

  // If compression was requested - setup decompressor
  if (compression)
    this._setupDecompressor(compression);

  var isConnectRequest = this._httpMessage &&
                         this._httpMessage.method === 'CONNECT';

  var info = {
    url: '',
    headers: headerList,
    versionMajor: 1,
    versionMinor: 1,
    method: false,
    statusCode: parseInt(headers.status, 10),
    statusMessage: headers.status.split(/ /)[1],
    upgrade: isConnectRequest
  };

  // Write data directly to the parser callbacks
  var onHeadersComplete = this._parserHeadersComplete();
  if (onHeadersComplete) {
    onHeadersComplete.call(this.parser, info);

    // onHeadersComplete doesn't handle CONNECT
    if (isConnectRequest) {
      var req = this._httpMessage;
      var res = this.parser.incoming;
      req.res = res;
      if (this.listeners('data').length) {
        // 0.11+ only (assuming nobody other than the http lib has attached)
        this.removeAllListeners('data');
        this.skipBodyParsing = true;
      }
      if (this._httpMessage.listeners('connect').length > 0)
        this._httpMessage.emit('connect', res, res.socket);
      else
        this.destroy();
    }
  } else {
    this.emit('headersComplete', info);
  }

  state.initialized = true;
};

//
// ### function setupDecompressor (type)
// #### @type {String} 'gzip' or 'deflate'
// Setup decompressor
//
Stream.prototype._setupDecompressor = function setupDecompressor(type) {
  var self = this;
  var state = this._spdyState;
  var options = { flush: zlib.Z_SYNC_FLUSH };

  if (state.decompressor !== null)
    return this.emit('error', new Error('Decompressor already created'));

  state.decompressor = type === 'gzip' ? zlib.createGunzip(options) :
                                         zlib.createInflate(options);
  if (spdy.utils.isLegacy)
    state.decompressor._flush = options.flush;
  state.decompressor.on('data', function(chunk) {
    self._recv(chunk, true);
  });
  state.decompressor.on('error', function(err) {
    self.emit('error', err);
  });
}

//
// ### function decompress(data)
// #### @data {Buffer} Input data
// Decompress input data and call `._recv(result, true)`
//
Stream.prototype._decompress = function decompress(data) {
  var state = this._spdyState;

  // Put data in
  state.decompressor.write(data);
};

//
// ### function write (data, encoding)
// #### @data {Buffer|String} data
// #### @encoding {String} data encoding
// Writes data to connection
//
Stream.prototype._write = function write(data, encoding, cb) {
  var r = true;
  var state = this._spdyState;

  // Ignore all outgoing data for PUSH streams, they're unidirectional
  if (state.isClient && state.associated) {
    cb();
    return r;
  }

  // First write is a client request
  if (state.parseRequest) {
    this._parseClientRequest(data, cb);
  } else {
    // No chunked encoding is allowed at this point
    assert(!this._httpMessage || !this._httpMessage.chunkedEncoding);

    // Do not send data to new connections after GOAWAY
    if (this._isGoaway()) {
      if (cb)
        cb();
      r = false;
    } else {
      r = this._writeData(false, data, cb);
    }
  }

  if (this._httpMessage && state.isClient && !state.finishAttached) {
    state.finishAttached = true;
    var self = this;

    // If client request was ended - send FIN data frame
    this._httpMessage.once('finish', function() {
      if (self._httpMessage.output &&
          !self._httpMessage.output.length  &&
          self._httpMessage.method !== 'CONNECT')
        self.end();
    });
  }

  return r;
};

if (spdy.utils.isLegacy) {
  Stream.prototype.write = function write(data, encoding, cb) {
    if (typeof encoding === 'function' && !cb) {
      cb = encoding;
      encoding = null;
    }
    if (!Buffer.isBuffer(data))
      return this._write(new Buffer(data, encoding), null, cb);
    else
      return this._write(data, encoding, cb);
  };

  //
  // ### function end (data, encoding, cb)
  // #### @data {Buffer|String} (optional) data to write before ending stream
  // #### @encoding {String} (optional) string encoding
  // #### @cb {Function}
  // Send FIN data frame
  //
  Stream.prototype.end = function end(data, encoding, cb) {
    // Do not send data to new connections after GOAWAY
    if (this._isGoaway())
      return;

    this._spdyState.ending = true;

    if (data)
      this.write(data, encoding, cb);
    this.emit('finish');
  };
} else {

  //
  // ### function end (data, encoding, cb)
  // #### @data {Buffer|String} (optional) data to write before ending stream
  // #### @encoding {String} (optional) string encoding
  // #### @cb {Function}
  // Send FIN data frame
  //
  Stream.prototype.end = function end(data, encoding, cb) {
    this._spdyState.ending = true;

    Stream.super_.prototype.end.call(this, data, encoding, cb);
  };
}

//
// ### function _recv (data, decompressed)
// #### @data {Buffer} buffer to receive
// #### @decompressed {Boolean} **internal** `true` if already decompressed
// (internal)
//
Stream.prototype._recv = function _recv(data, decompressed) {
  var state = this._spdyState;

  // Update window if exhausted
  if (state.framer.version >= 3 && state.initialized) {
    state.windowSize -= data.length;

    // If running on node.js 0.8 - don't send WINDOW_UPDATE if paused
    if (spdy.utils.isLegacy && !state.paused)
      this._read();
  }

  // Decompress data if needed
  if (state.decompressor && !decompressed)
    return this._decompress(data);

  // Reset timeout
  this.setTimeout();

  if (this.parser && !this.skipBodyParsing) {
    var onBody = this._parserBody();
    if (onBody)
      onBody.call(this.parser, data, 0, data.length);
  }

  if (spdy.utils.isLegacy) {
    var self = this;
    utils.nextTick(function() {
      self.emit('data', data);
      if (self.ondata && !onBody)
        self.ondata(data, 0, data.length);
    });
  } else {
    // 0.11 - 0.12 - do not emit events at all
    if (!onBody || !this.parser[2]) {
      // Right now, http module expects socket to be working in streams1 mode.
      if (this.ondata && !onBody)
        this.ondata(data, 0, data.length);
      else
        this.push(data);
    }
  }

  // Call `._read()` if high watermark isn't reached
  if (!spdy.utils.isLegacy)
    this.read(0);
};

//
// ### function _recvEnd (callback, quite)
// #### @callback {Function}
// #### @quite {Boolean} If true - do not emit any events
// Receive FIN frame
//
Stream.prototype._recvEnd = function _recvEnd(callback, quite) {
  var state = this._spdyState;

  // Sync with the decompressor
  if (state.decompressor) {
    var self = this;
    state.decompressor.write('', function() {
      state.decompressor = null;
      self._recvEnd(callback, quite);
    });
    return;
  }
  if (!quite) {
    var onMessageComplete = this._parserMessageComplete();
    if (onMessageComplete)
      onMessageComplete.call(this.parser);

    if (spdy.utils.isLegacy)
      this.emit('end');
    else
      this.push(null);
  }
  if (callback)
    callback();
};

//
// ### function _read (bytes)
// #### @bytes {Number} number of bytes to read
// Streams2 API
//
Stream.prototype._read = function read(bytes) {
  var state = this._spdyState;

  // Send update frame if read is requested
  if (state.framer.version >= 3 &&
      state.initialized &&
      state.windowSize <= state.initialWindowSize / 2) {
    var delta = state.initialWindowSize - state.windowSize;
    state.windowSize += delta;
    var self = this;
    state.framer.windowUpdateFrame(state.id, delta, function(err, frame) {
      if (err)
        return self.emit('error', err);
      self.connection.write(frame);
    });
  }

  if (!spdy.utils.isLegacy)
    this.push('');
};

//
// ### function _updateSinkSize (size)
// #### @size {Integer}
// Update the internal data transfer window
//
Stream.prototype._updateSinkSize = function _updateSinkSize(size) {
  var state = this._spdyState;
  var delta = size - state.initialSinkSize;

  state.initialSinkSize = size;
  this._drainSink(delta);
};

//
// ### function lock (callback)
// #### @callback {Function} continuation callback
// Acquire lock
//
Stream.prototype._lock = function lock(callback) {
  if (!callback)
    return;

  var self = this;
  this.connection._lock(function(err) {
    callback.call(self, err);
  });
};

//
// ### function unlock ()
// Release lock and call all buffered callbacks
//
Stream.prototype._unlock = function unlock() {
  this.connection._unlock();
};

//
// `net` compatibility layer
// (Copy pasted from lib/tls.js from node.js)
//
Stream.prototype.address = function address() {
  return this.socket && this.socket.address();
};

Stream.prototype.__defineGetter__('remoteAddress', function remoteAddress() {
  return this.socket && this.socket.remoteAddress;
});

Stream.prototype.__defineGetter__('remotePort', function remotePort() {
  return this.socket && this.socket.remotePort;
});

Stream.prototype.setNoDelay = function setNoDelay(enable) {
  return this.socket && this.socket.setNoDelay(enable);
};

Stream.prototype.setKeepAlive = function(setting, msecs) {
  return this.socket && this.socket.setKeepAlive(setting, msecs);
};

Stream.prototype.getPeerCertificate = function() {
  return this.socket && this.socket.getPeerCertificate();
};

Stream.prototype.getSession = function() {
  return this.socket && this.socket.getSession();
};

Stream.prototype.isSessionReused = function() {
  return this.socket && this.socket.isSessionReused();
};

Stream.prototype.getCipher = function() {
  return this.socket && this.socket.getCipher();
};
