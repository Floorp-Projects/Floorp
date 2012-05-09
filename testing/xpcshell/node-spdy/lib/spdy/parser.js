var parser = exports;

var spdy = require('../spdy'),
    util = require('util'),
    stream = require('stream'),
    Buffer = require('buffer').Buffer;

//
// ### function Parser (connection, deflate, inflate)
// #### @connection {spdy.Connection} connection
// #### @deflate {zlib.Deflate} Deflate stream
// #### @inflate {zlib.Inflate} Inflate stream
// SPDY protocol frames parser's @constructor
//
function Parser(connection, deflate, inflate) {
  stream.Stream.call(this);

  this.drained = true;
  this.paused = false;
  this.buffer = [];
  this.buffered = 0;
  this.waiting = 8;

  this.state = { type: 'frame-head' };
  this.deflate = deflate;
  this.inflate = inflate;
  this.framer = null;

  this.connection = connection;

  this.readable = this.writable = true;
}
util.inherits(Parser, stream.Stream);

//
// ### function create (connection, deflate, inflate)
// #### @connection {spdy.Connection} connection
// #### @deflate {zlib.Deflate} Deflate stream
// #### @inflate {zlib.Inflate} Inflate stream
// @constructor wrapper
//
parser.create = function create(connection, deflate, inflate) {
  return new Parser(connection, deflate, inflate);
};

//
// ### function write (data)
// #### @data {Buffer} chunk of data
// Writes or buffers data to parser
//
Parser.prototype.write = function write(data) {
  if (data !== undefined) {
    // Buffer data
    this.buffer.push(data);
    this.buffered += data.length;
  }

  // Notify caller about state (for piping)
  if (this.paused) return false;

  // We shall not do anything until we get all expected data
  if (this.buffered < this.waiting) return;

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
    if (err) return self.emit('error', err);

    // Set new `waiting`
    self.waiting = waiting;

    if (self.waiting <= self.buffered) {
      self.write();
    } else {
      process.nextTick(function() {
        if (self.drained) return;

        // Mark parser as drained
        self.drained = true;
        self.emit('drain');
      });
    }
  });
};

//
// ### function end ()
// Stream's end() implementation
//
Parser.prototype.end = function end() {
  this.emit('end');
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
      if (spdy.protocol[header.version]) {
        this.framer = new spdy.protocol[header.version].Framer(
          spdy.utils.zwrap(this.deflate),
          spdy.utils.zwrap(this.inflate)
        );

        // Propagate framer to connection
        this.connection.framer = this.framer;
        this.emit('_framer', this.framer);
      }
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
