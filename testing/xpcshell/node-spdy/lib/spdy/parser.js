var parser = exports;

var spdy = require('../spdy'),
    util = require('util'),
    stream = require('stream'),
    Buffer = require('buffer').Buffer;

var legacy = !stream.Duplex;

if (legacy) {
  var DuplexStream = stream;
} else {
  var DuplexStream = stream.Duplex;
}

//
// ### function Parser (connection)
// #### @connection {spdy.Connection} connection
// SPDY protocol frames parser's @constructor
//
function Parser(connection) {
  DuplexStream.call(this);

  this.drained = true;
  this.paused = false;
  this.buffer = [];
  this.buffered = 0;
  this.waiting = 8;

  this.state = { type: 'frame-head' };
  this.socket = connection.socket;
  this.connection = connection;
  this.framer = null;

  this.connection = connection;

  if (legacy) {
    this.readable = this.writable = true;
  }
}
util.inherits(Parser, DuplexStream);

//
// ### function create (connection)
// #### @connection {spdy.Connection} connection
// @constructor wrapper
//
parser.create = function create(connection) {
  return new Parser(connection);
};

//
// ### function destroy ()
// Just a stub.
//
Parser.prototype.destroy = function destroy() {
};

//
// ### function _write (data, encoding, cb)
// #### @data {Buffer} chunk of data
// #### @encoding {Null} encoding
// #### @cb {Function} callback
// Writes or buffers data to parser
//
Parser.prototype._write = function write(data, encoding, cb) {
  // Legacy compatibility
  if (!cb) cb = function() {};

  if (data !== undefined) {
    // Buffer data
    this.buffer.push(data);
    this.buffered += data.length;
  }

  // Notify caller about state (for piping)
  if (this.paused) return false;

  // We shall not do anything until we get all expected data
  if (this.buffered < this.waiting) return cb();

  // Mark parser as not drained
  if (data !== undefined) this.drained = false;

  var self = this,
      buffer = new Buffer(this.waiting),
      sliced = 0,
      offset = 0;

  while (this.waiting > offset && sliced < this.buffer.length) {
    var chunk = this.buffer[sliced++],
        overmatched = false;

    // Copy chunk into `buffer`
    if (chunk.length > this.waiting - offset) {
      chunk.copy(buffer, offset, 0, this.waiting - offset);

      this.buffer[--sliced] = chunk.slice(this.waiting - offset);
      this.buffered += this.buffer[sliced].length;

      overmatched = true;
    } else {
      chunk.copy(buffer, offset);
    }

    // Move offset and decrease amount of buffered data
    offset += chunk.length;
    this.buffered -= chunk.length;

    if (overmatched) break;
  }

  // Remove used buffers
  this.buffer = this.buffer.slice(sliced);

  // Executed parser for buffered data
  this.paused = true;
  this.execute(this.state, buffer, function (err, waiting) {
    // And unpause once execution finished
    self.paused = false;

    // Propagate errors
    if (err) {
      cb();
      return self.emit('error', err);
    }

    // Set new `waiting`
    self.waiting = waiting;

    if (self.waiting <= self.buffered) {
      self._write(undefined, null, cb);
    } else {
      process.nextTick(function() {
        if (self.drained) return;

        // Mark parser as drained
        self.drained = true;
        self.emit('drain');
      });

      cb();
    }
  });
};

if (legacy) {
  //
  // ### function write (data, encoding, cb)
  // #### @data {Buffer} chunk of data
  // #### @encoding {Null} encoding
  // #### @cb {Function} callback
  // Legacy method
  //
  Parser.prototype.write = Parser.prototype._write;

  //
  // ### function end ()
  // Stream's end() implementation
  //
  Parser.prototype.end = function end() {
    this.emit('end');
  };
}

//
// ### function createFramer (version)
// #### @version {Number} Protocol version, either 2 or 3
// Sets framer instance on Parser's instance
//
Parser.prototype.createFramer = function createFramer(version) {
  if (spdy.protocol[version]) {
    this.emit('version', version);

    this.framer = new spdy.protocol[version].Framer(
      spdy.utils.zwrap(this.connection._deflate),
      spdy.utils.zwrap(this.connection._inflate)
    );

    // Propagate framer to connection
    this.connection._framer = this.framer;
    this.emit('framer', this.framer);
  } else {
    this.emit(
      'error',
      new Error('Unknown protocol version requested: ' + version)
    );
  }
};

//
// ### function execute (state, data, callback)
// #### @state {Object} Parser's state
// #### @data {Buffer} Incoming data
// #### @callback {Function} continuation callback
// Parse buffered data
//
Parser.prototype.execute = function execute(state, data, callback) {
  if (state.type === 'frame-head') {
    var header = state.header = spdy.protocol.generic.parseHeader(data);

    // Lazily create framer
    if (!this.framer && header.control) {
      this.createFramer(header.version);
    }

    state.type = 'frame-body';
    callback(null, header.length);
  } else if (state.type === 'frame-body') {
    var self = this;

    // Data frame
    if (!state.header.control) {
      return onFrame(null, {
        type: 'DATA',
        id: state.header.id,
        fin: (state.header.flags & 0x01) === 0x01,
        compressed: (state.header.flags & 0x02) === 0x02,
        data: data
      });
    } else {
    // Control frame
      this.framer.execute(state.header, data, onFrame);
    }

    function onFrame(err, frame) {
      if (err) return callback(err);

      self.emit('frame', frame);

      state.type = 'frame-head';
      callback(null, 8);
    };
  }
};
