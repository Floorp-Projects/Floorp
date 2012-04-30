var spdy = require('../spdy'),
    util = require('util'),
    https = require('https'),
    stream = require('stream'),
    Buffer = require('buffer').Buffer;

//
// ### function instantiate (HTTPSServer)
// #### @HTTPSServer {https.Server|Function} Base server class
// Will return constructor for SPDY Server, based on the HTTPSServer class
//
function instantiate(HTTPSServer) {
  //
  // ### function Server (options, requestListener)
  // #### @options {Object} tls server options
  // #### @requestListener {Function} (optional) request callback
  // SPDY Server @constructor
  //
  function Server(options, requestListener) {
    if (!options) options = {};
    if (!options.maxStreams) options.maxStreams = 100;
    options.NPNProtocols = ['spdy/2', 'http/1.1', 'http/1.0'];

    HTTPSServer.call(this, options, requestListener);

    // Use https if NPN is not supported
    if (!process.features.tls_npn && !options.debug) return;

    // Wrap connection handler
    var self = this,
        connectionHandler = this.listeners('secureConnection')[0];

    var pool = spdy.zlibpool.create();

    this.removeAllListeners('secureConnection');
    this.on('secureConnection', function secureConnection(socket) {
      // Fallback to HTTPS if needed
      if (socket.npnProtocol !== 'spdy/2' && !options.debug) {
        return connectionHandler.call(this, socket);
      }

      // Wrap incoming socket into abstract class
      var connection = new Connection(socket, pool, options);

      // Emulate each stream like connection
      connection.on('stream', connectionHandler);

      connection.on('request', function(req, res) {
        res._renderHeaders = spdy.response._renderHeaders;
        res.writeHead = spdy.response.writeHead;
        res.push = spdy.response.push;
        res.streamID = req.streamID = req.socket.id;
        res.isSpdy = req.isSpdy = true;

        res.on('finish', function() {
          req.connection.end();
        });
        self.emit('request', req, res);
      });

      connection.on('error', function(e) {
        socket.destroy(e.errno === 'EPIPE' ? undefined : e);
      });
    });
  }
  util.inherits(Server, HTTPSServer);

  return Server;
}
exports.instantiate = instantiate;

// Export Server instantiated from https.Server
var Server = instantiate(https.Server);
exports.Server = Server;

//
// ### function create (base, options, requestListener)
// #### @base {Function} (optional) base server class (https.Server)
// #### @options {Object} tls server options
// #### @requestListener {Function} (optional) request callback
// @constructor wrapper
//
exports.create = function create(base, options, requestListener) {
  var server;
  if (typeof base === 'function') {
    server = instantiate(base);
  } else {
    server = Server;

    requestListener = options;
    options = base;
    base = null;
  }

  return new server(options, requestListener);
};

//
// ### function Connection (socket, pool, options)
// #### @socket {net.Socket} server's connection
// #### @pool {spdy.ZlibPool} zlib pool
// #### @options {Object} server's options
// Abstract connection @constructor
//
function Connection(socket, pool, options) {
  process.EventEmitter.call(this);

  var self = this;

  this._closed = false;

  this.pool = pool;
  var pair = pool.get();

  this.deflate = pair.deflate;
  this.inflate = pair.inflate;

  // Init streams list
  this.streams = {};
  this.streamsCount = 0;
  this.pushId = 0;
  this.goaway = false;

  this.framer = null;

  // Initialize parser
  this.parser = spdy.parser.create(this, this.deflate, this.inflate);
  this.parser.on('frame', function (frame) {
    if (this._closed) return;

    var stream;

    // Create new stream
    if (frame.type === 'SYN_STREAM') {
      self.streamsCount++;

      stream = self.streams[frame.id] = new Stream(self, frame);

      // If we reached stream limit
      if (self.streamsCount > options.maxStreams) {
        stream.once('error', function() {});
        // REFUSED_STREAM
        stream.rstCode = 3;
        stream.destroy(true);
      } else {
        self.emit('stream', stream);

        stream.init();
      }
    } else {
      if (frame.id) {
        // Load created one
        stream = self.streams[frame.id];

        // Fail if not found
        if (stream === undefined) {
          if (frame.type === 'RST_STREAM') return;
          return self.emit('error', 'Stream ' + frame.id + ' not found');
        }
      }

      // Emit 'data' event
      if (frame.type === 'DATA') {
        if (frame.data.length > 0){
          if (stream.closedBy.client) {
            stream.rstCode = 2;
            stream.emit('error', 'Writing to half-closed stream');
          } else {
            stream._read(frame.data);
          }
        }
      // Destroy stream if we was asked to do this
      } else if (frame.type === 'RST_STREAM') {
        stream.rstCode = 0;
        if (frame.status === 5) {
          // If client "cancels" connection - close stream and
          // all associated push streams without error
          stream.pushes.forEach(function(stream) {
            stream.close();
          });
          stream.close();
        } else {
          // Emit error on destroy
          stream.destroy(new Error('Received rst: ' + frame.status));
        }
      // Respond with same PING
      } else if (frame.type === 'PING') {
        self.write(self.framer.pingFrame(frame.pingId));
      // Ignore SETTINGS for now
      } else if (frame.type === 'SETTINGS' || frame.type === 'NOOP') {
        // TODO: Handle something?
      } else if (frame.type === 'GOAWAY') {
        self.goaway = frame.lastId;
      } else {
        console.error('Unknown type: ', frame.type);
      }
    }

    // Handle half-closed
    if (frame.fin) {
      // Don't allow to close stream twice
      if (stream.closedBy.client) {
        stream.rstCode = 2;
        stream.emit('error', 'Already half-closed');
      } else {
        stream.closedBy.client = true;
        stream.handleClose();
      }
    }
  });

  this.parser.on('_framer', function(framer) {
    // Generate custom settings frame and send
    self.write(
      framer.maxStreamsFrame(options.maxStreams)
    );
  });

  // Initialize scheduler
  this.scheduler = spdy.scheduler.create(this);

  // Store socket and pipe it to parser
  this.socket = socket;

  socket.pipe(this.parser);

  // 2 minutes socket timeout
  socket.setTimeout(2 * 60 * 1000);
  socket.once('timeout', function() {
    socket.destroy();
  });

  // Allow high-level api to catch socket errors
  socket.on('error', function(e) {
    self.emit('error', e);
  });

  socket.on('close', function() {
    self._closed = true;
    pool.put(pair);
  });

  socket.on('drain', function () {
    self.emit('drain');
  });
}
util.inherits(Connection, process.EventEmitter);
exports.Connection = Connection;

//
// ### function write (data, encoding)
// #### @data {String|Buffer} data
// #### @encoding {String} (optional) encoding
// Writes data to socket
//
Connection.prototype.write = function write(data, encoding) {
  if (this.socket.writable) {
    return this.socket.write(data, encoding);
  }
};

//
// ### function Stream (connection, frame)
// #### @connection {Connection} SPDY Connection
// #### @frame {Object} SYN_STREAM data
// Abstract stream @constructor
//
function Stream(connection, frame) {
  var self = this;
  stream.Stream.call(this);

  this.connection = connection;
  this.framer = connection.framer;

  this.ondata = this.onend = null;

  // RST_STREAM code if any
  this.rstCode = 1;
  this._destroyed = false;

  this.closedBy = {
    client: false,
    server: false
  };

  // Lock data
  this.locked = false;
  this.lockBuffer = [];

  // Store id
  this.id = frame.id;

  // Store priority
  this.priority = frame.priority;

  // Array of push streams associated to that one
  this.pushes = [];

  this._paused = false;
  this._buffer = [];

  // Create compression streams
  this.deflate = connection.deflate;
  this.inflate = connection.inflate;

  // Store headers
  this.headers = frame.headers;

  this.frame = frame;

  this.readable = this.writable = true;
}
util.inherits(Stream, stream.Stream);
exports.Stream = Stream;

//
// ### function isGoaway ()
// Returns true if any writes to that stream should be ignored
//
Stream.prototype.isGoaway = function isGoaway() {
  return this.connection.goaway && this.id > this.connection.goaway;
};

//
// ### function init ()
// Initialize stream, internal
//
Stream.prototype.init = function init() {
  var headers = this.headers,
      req = [headers.method + ' ' + headers.url + ' ' + headers.version];

  Object.keys(headers).forEach(function (key) {
    if (key !== 'method' && key !== 'url' && key !== 'version' &&
        key !== 'scheme') {
      req.push(key + ': ' + headers[key]);
    }
  });

  // Add '\r\n\r\n'
  req.push('', '');

  req = new Buffer(req.join('\r\n'));

  this._read(req);
};

//
// ### function lock (callback)
// #### @callback {Function} continuation callback
// Acquire lock
//
Stream.prototype.lock = function lock(callback) {
  if (!callback) return;

  if (this.locked) {
    this.lockBuffer.push(callback);
  } else {
    this.locked = true;
    callback.call(this, null);
  }
};

//
// ### function unlock ()
// Release lock and call all buffered callbacks
//
Stream.prototype.unlock = function unlock() {
  if (this.locked) {
    this.locked = false;
    this.lock(this.lockBuffer.shift());
  }
};

//
// ### function setTimeout ()
// TODO: use timers.enroll, timers.active, timers.unenroll
//
Stream.prototype.setTimeout = function setTimeout(time) {};

//
// ### function handleClose ()
// Close stream if it was closed by both server and client
//
Stream.prototype.handleClose = function handleClose() {
  if (this.closedBy.client && this.closedBy.server) {
    this.close();
  }
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
  if (this._destroyed) return;
  this._destroyed = true;

  delete this.connection.streams[this.id];
  this.connection.streamsCount--;

  if (error) {
    if (this.rstCode) {
      this.connection.write(this.framer.rstFrame(this.id, this.rstCode));
    }
  }

  var self = this;
  process.nextTick(function() {
    if (error) self.emit('error', error);
    self.emit('close');
  });
};

//
// ### function _writeData (fin, buffer)
// #### @fin {Boolean}
// #### @buffer {Buffer}
// Internal function
//
Stream.prototype._writeData = function _writeData(fin, buffer) {
  this.lock(function() {
    var stream = this,
        frame = this.framer.dataFrame(this.id, fin, buffer);

    stream.connection.scheduler.schedule(stream, frame);
    stream.connection.scheduler.tick();

    if (fin) this.close();

    this.unlock();
  });
};

//
// ### function write (data, encoding)
// #### @data {Buffer|String} data
// #### @encoding {String} data encoding
// Writes data to connection
//
Stream.prototype.write = function write(data, encoding) {
  // Do not send data to new connections after GOAWAY
  if (this.isGoaway()) return;

  var buffer;

  // Send DATA
  if (typeof data === 'string') {
    buffer = new Buffer(data, encoding);
  } else {
    buffer = data;
  }

  // TCP Frame size is equal to:
  // data frame size + data frame headers (8 bytes) +
  // tls frame headers and data (encrypted) ~ 140 bytes
  // So setting MTU equal to 1300 is most optimal
  // as end packet will have size ~ 1442 bytes
  // (My MTU is 1492 bytes)
  var MTU = 1300;

  // Try to fit MTU
  if (buffer.length < MTU) {
    this._writeData(false, buffer);
  } else {
    var len = buffer.length - MTU;
    for (var offset = 0; offset < len; offset += MTU) {
      this._writeData(false, buffer.slice(offset, offset + MTU));
    }
    this._writeData(false, buffer.slice(offset));
  }
};

//
// ### function end (data)
// #### @data {Buffer|String} (optional) data to write before ending stream
// #### @encoding {String} (optional) string encoding
// Send FIN data frame
//
Stream.prototype.end = function end(data, encoding) {
  // Do not send data to new connections after GOAWAY
  if (this.isGoaway()) return;

  if (data) this.write(data, encoding);

  this._writeData(true, []);
  this.closedBy.server = true;
  this.handleClose();
};

//
// ### function pause ()
// Start buffering all data
//
Stream.prototype.pause = function pause() {
  if (this._paused) return;
  this._paused = true;
};

//
// ### function resume ()
// Start buffering all data
//
Stream.prototype.resume = function resume() {
  if (!this._paused) return;
  this._paused = false;

  var self = this,
      buffer = this._buffer;

  this._buffer = [];

  process.nextTick(function() {
    buffer.forEach(function(chunk) {
      self._read(chunk);
    });
  });
};

//
// ### function _read (data)
// #### @data {Buffer} buffer to read
// (internal)
//
Stream.prototype._read = function _read(data) {
  if (this._paused) {
    this._buffer.push(data);
    return;
  }

  var self = this;
  process.nextTick(function() {
    if (self.ondata) self.ondata(data, 0, data.length);
    self.emit('data', data);
  });
};
