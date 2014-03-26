// The framer consists of two [Transform Stream][1] subclasses that operate in [object mode][2]:
// the Serializer and the Deserializer
// [1]: http://nodejs.org/api/stream.html#stream_class_stream_transform
// [2]: http://nodejs.org/api/stream.html#stream_new_stream_readable_options
var assert = require('assert');

var Transform = require('stream').Transform;

exports.Serializer = Serializer;
exports.Deserializer = Deserializer;

var logData = Boolean(process.env.HTTP2_LOG_DATA);

var MAX_PAYLOAD_SIZE = 16383;

// Serializer
// ----------
//
//     Frame Objects
//     * * * * * * * --+---------------------------
//                     |                          |
//                     v                          v           Buffers
//      [] -----> Payload Ser. --[buffers]--> Header Ser. --> * * * *
//     empty      adds payload                adds header
//     array        buffers                     buffer

function Serializer(log, sizeLimit) {
  this._log = log.child({ component: 'serializer' });
  this._sizeLimit = sizeLimit || MAX_PAYLOAD_SIZE;
  Transform.call(this, { objectMode: true });
}
Serializer.prototype = Object.create(Transform.prototype, { constructor: { value: Serializer } });

// When there's an incoming frame object, it first generates the frame type specific part of the
// frame (payload), and then then adds the header part which holds fields that are common to all
// frame types (like the length of the payload).
Serializer.prototype._transform = function _transform(frame, encoding, done) {
  this._log.trace({ frame: frame }, 'Outgoing frame');

  assert(frame.type in Serializer, 'Unknown frame type: ' + frame.type);

  var buffers = [];
  Serializer[frame.type](frame, buffers);
  Serializer.commonHeader(frame, buffers);

  assert(buffers[0].readUInt16BE(0) <= this._sizeLimit, 'Frame too large!');

  for (var i = 0; i < buffers.length; i++) {
    if (logData) {
      this._log.trace({ data: buffers[i] }, 'Outgoing data');
    }
    this.push(buffers[i]);
  }

  done();
};

// Deserializer
// ------------
//
//     Buffers
//     * * * * --------+-------------------------
//                     |                        |
//                     v                        v           Frame Objects
//      {} -----> Header Des. --{frame}--> Payload Des. --> * * * * * * *
//     empty      adds parsed              adds parsed
//     object  header properties        payload properties

function Deserializer(log, sizeLimit, role) {
  this._role = role;
  this._log = log.child({ component: 'deserializer' });
  this._sizeLimit = sizeLimit || MAX_PAYLOAD_SIZE;
  Transform.call(this, { objectMode: true });
  this._next(COMMON_HEADER_SIZE);
}
Deserializer.prototype = Object.create(Transform.prototype, { constructor: { value: Deserializer } });

// The Deserializer is stateful, and it's two main alternating states are: *waiting for header* and
// *waiting for payload*. The state is stored in the boolean property `_waitingForHeader`.
//
// When entering a new state, a `_buffer` is created that will hold the accumulated data (header or
// payload). The `_cursor` is used to track the progress.
Deserializer.prototype._next = function(size) {
  this._cursor = 0;
  this._buffer = new Buffer(size);
  this._waitingForHeader = !this._waitingForHeader;
  if (this._waitingForHeader) {
    this._frame = {};
  }
};

// Parsing an incoming buffer is an iterative process because it can hold multiple frames if it's
// large enough. A `cursor` is used to track the progress in parsing the incoming `chunk`.
Deserializer.prototype._transform = function _transform(chunk, encoding, done) {
  var cursor = 0;

  if (logData) {
    this._log.trace({ data: chunk }, 'Incoming data');
  }

  while(cursor < chunk.length) {
    // The content of an incoming buffer is first copied to `_buffer`. If it can't hold the full
    // chunk, then only a part of it is copied.
    var toCopy = Math.min(chunk.length - cursor, this._buffer.length - this._cursor);
    chunk.copy(this._buffer, this._cursor, cursor, cursor + toCopy);
    this._cursor += toCopy;
    cursor += toCopy;

    // When `_buffer` is full, it's content gets parsed either as header or payload depending on
    // the actual state.

    // If it's header then the parsed data is stored in a temporary variable and then the
    // deserializer waits for the specified length payload.
    if ((this._cursor === this._buffer.length) && this._waitingForHeader) {
      var payloadSize = Deserializer.commonHeader(this._buffer, this._frame);
      if (payloadSize <= this._sizeLimit) {
        this._next(payloadSize);
      } else {
        this.emit('error', 'FRAME_SIZE_ERROR');
        return;
      }
    }

    // If it's payload then the the frame object is finalized and then gets pushed out.
    // Unknown frame types are ignored.
    //
    // Note: If we just finished the parsing of a header and the payload length is 0, this branch
    // will also run.
    if ((this._cursor === this._buffer.length) && !this._waitingForHeader) {
      if (this._frame.type) {
        var error = Deserializer[this._frame.type](this._buffer, this._frame, this._role);
        if (error) {
          this._log.error('Incoming frame parsing error: ' + error);
          this.emit('error', 'PROTOCOL_ERROR');
        } else {
          this._log.trace({ frame: this._frame }, 'Incoming frame');
          this.push(this._frame);
        }
      } else {
        this._log.error('Unknown type incoming frame');
        this.emit('error', 'PROTOCOL_ERROR');
      }
      this._next(COMMON_HEADER_SIZE);
    }
  }

  done();
};

// [Frame Header](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-4.1)
// --------------------------------------------------------------
//
// HTTP/2.0 frames share a common base format consisting of an 8-byte header followed by 0 to 65535
// bytes of data.
//
// Additional size limits can be set by specific application uses. HTTP limits the frame size to
// 16,383 octets.
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     | R |     Length (14)           |   Type (8)    |   Flags (8)   |
//     +-+-+---------------------------+---------------+---------------+
//     |R|                 Stream Identifier (31)                      |
//     +-+-------------------------------------------------------------+
//     |                     Frame Data (0...)                       ...
//     +---------------------------------------------------------------+
//
// The fields of the frame header are defined as:
//
// * R:
//   A reserved 2-bit field. The semantics of these bits are undefined and the bits MUST remain
//   unset (0) when sending and MUST be ignored when receiving.
//
// * Length:
//   The length of the frame data expressed as an unsigned 14-bit integer. The 8 bytes of the frame
//   header are not included in this value.
//
// * Type:
//   The 8-bit type of the frame. The frame type determines how the remainder of the frame header
//   and data are interpreted. Implementations MUST ignore unsupported and unrecognized frame types.
//
// * Flags:
//   An 8-bit field reserved for frame-type specific boolean flags.
//
//   Flags are assigned semantics specific to the indicated frame type. Flags that have no defined
//   semantics for a particular frame type MUST be ignored, and MUST be left unset (0) when sending.
//
// * R:
//   A reserved 1-bit field. The semantics of this bit are undefined and the bit MUST remain unset
//   (0) when sending and MUST be ignored when receiving.
//
// * Stream Identifier:
//   A 31-bit stream identifier. The value 0 is reserved for frames that are associated with the
//   connection as a whole as opposed to an individual stream.
//
// The structure and content of the remaining frame data is dependent entirely on the frame type.

var COMMON_HEADER_SIZE = 8;

var frameTypes = [];

var frameFlags = {};

var genericAttributes = ['type', 'flags', 'stream'];

var typeSpecificAttributes = {};

Serializer.commonHeader = function writeCommonHeader(frame, buffers) {
  var headerBuffer = new Buffer(COMMON_HEADER_SIZE);

  var size = 0;
  for (var i = 0; i < buffers.length; i++) {
    size += buffers[i].length;
  }
  headerBuffer.writeUInt16BE(size, 0);

  var typeId = frameTypes.indexOf(frame.type);  // If we are here then the type is valid for sure
  headerBuffer.writeUInt8(typeId, 2);

  var flagByte = 0;
  for (var flag in frame.flags) {
    var position = frameFlags[frame.type].indexOf(flag);
    assert(position !== -1, 'Unknown flag for frame type ' + frame.type + ': ' + flag);
    if (frame.flags[flag]) {
      flagByte |= (1 << position);
    }
  }
  headerBuffer.writeUInt8(flagByte, 3);

  assert((0 <= frame.stream) && (frame.stream < 0x7fffffff), frame.stream);
  headerBuffer.writeUInt32BE(frame.stream || 0, 4);

  buffers.unshift(headerBuffer);
};

Deserializer.commonHeader = function readCommonHeader(buffer, frame) {
  var length = buffer.readUInt16BE(0);

  frame.type = frameTypes[buffer.readUInt8(2)];

  frame.flags = {};
  var flagByte = buffer.readUInt8(3);
  var definedFlags = frameFlags[frame.type];
  for (var i = 0; i < definedFlags.length; i++) {
    frame.flags[definedFlags[i]] = Boolean(flagByte & (1 << i));
  }

  frame.stream = buffer.readUInt32BE(4) & 0x7fffffff;

  return length;
};

// Frame types
// ===========

// Every frame type is registered in the following places:
//
// * `frameTypes`: a register of frame type codes (used by `commonHeader()`)
// * `frameFlags`: a register of valid flags for frame types (used by `commonHeader()`)
// * `typeSpecificAttributes`: a register of frame specific frame object attributes (used by
//   logging code and also serves as documentation for frame objects)

// [DATA Frames](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.1)
// ------------------------------------------------------------
//
// DATA frames (type=0x0) convey arbitrary, variable-length sequences of octets associated with a
// stream.
//
// The DATA frame defines the following flags:
//
// * END_STREAM (0x1):
//   Bit 1 being set indicates that this frame is the last that the endpoint will send for the
//   identified stream.
// * END_SEGMENT (0x2):
//   Bit 2 being set indicates that this frame is the last for the current segment. Intermediaries
//   MUST NOT coalesce frames across a segment boundary and MUST preserve segment boundaries when
//   forwarding frames.
// * PAD_LOW (0x10):
//   Bit 5 being set indicates that the Pad Low field is present.
// * PAD_HIGH (0x20):
//   Bit 6 being set indicates that the Pad High field is present. This bit MUST NOT be set unless
//   the PAD_LOW flag is also set. Endpoints that receive a frame with PAD_HIGH set and PAD_LOW
//   cleared MUST treat this as a connection error of type PROTOCOL_ERROR.

frameTypes[0x0] = 'DATA';

frameFlags.DATA = ['END_STREAM', 'END_SEGMENT', 'RESERVED4', 'RESERVED8', 'PAD_LOW', 'PAD_HIGH'];

typeSpecificAttributes.DATA = ['data'];

Serializer.DATA = function writeData(frame, buffers) {
  buffers.push(frame.data);
};

Deserializer.DATA = function readData(buffer, frame) {
  var dataOffset = 0;
  var paddingLength = 0;
  if (frame.flags.PAD_LOW) {
    if (frame.flags.PAD_HIGH) {
      paddingLength = (buffer.readUInt8(dataOffset) & 0xff) * 256;
      dataOffset += 1;
    }
    paddingLength += (buffer.readUInt8(dataOffset) & 0xff);
    dataOffset += 1;
  } else if (frame.flags.PAD_HIGH) {
    return 'DATA frame got PAD_HIGH without PAD_LOW';
  }
  if (paddingLength) {
    frame.data = buffer.slice(dataOffset, -1 * paddingLength);
  } else {
    frame.data = buffer.slice(dataOffset);
  }
};

// [HEADERS](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.2)
// --------------------------------------------------------------
//
// The HEADERS frame (type=0x1) allows the sender to create a stream.
//
// The HEADERS frame defines the following flags:
//
// * END_STREAM (0x1):
//   Bit 1 being set indicates that this frame is the last that the endpoint will send for the
//   identified stream.
// * END_SEGMENT (0x2):
//   Bit 2 being set indicates that this frame is the last for the current segment. Intermediaries
//   MUST NOT coalesce frames across a segment boundary and MUST preserve segment boundaries when
//   forwarding frames.
// * END_HEADERS (0x4):
//   The END_HEADERS bit indicates that this frame contains the entire payload necessary to provide
//   a complete set of headers.
// * PRIORITY (0x8):
//   Bit 4 being set indicates that the first four octets of this frame contain a single reserved
//   bit and a 31-bit priority.
// * PAD_LOW (0x10):
//   Bit 5 being set indicates that the Pad Low field is present.
// * PAD_HIGH (0x20):
//   Bit 6 being set indicates that the Pad High field is present. This bit MUST NOT be set unless
//   the PAD_LOW flag is also set. Endpoints that receive a frame with PAD_HIGH set and PAD_LOW
//   cleared MUST treat this as a connection error of type PROTOCOL_ERROR.

frameTypes[0x1] = 'HEADERS';

frameFlags.HEADERS = ['END_STREAM', 'END_SEGMENT', 'END_HEADERS', 'PRIORITY', 'PAD_LOW', 'PAD_HIGH'];

typeSpecificAttributes.HEADERS = ['priority', 'headers', 'data'];

//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |X|               (Optional) Priority (31)                      |
//     +-+-------------------------------------------------------------+
//     |                    Header Block (*)                         ...
//     +---------------------------------------------------------------+
//
// The payload of a HEADERS frame contains a Headers Block

Serializer.HEADERS = function writeHeadersPriority(frame, buffers) {
  if (frame.flags.PRIORITY) {
    var buffer = new Buffer(4);
    assert((0 <= frame.priority) && (frame.priority <= 0xffffffff), frame.priority);
    buffer.writeUInt32BE(frame.priority, 0);
    buffers.push(buffer);
  }
  buffers.push(frame.data);
};

Deserializer.HEADERS = function readHeadersPriority(buffer, frame) {
  var dataOffset = 0;
  var paddingLength = 0;
  if (frame.flags.PAD_LOW) {
    if (frame.flags.PAD_HIGH) {
      paddingLength = (buffer.readUInt8(dataOffset) & 0xff) * 256;
      dataOffset += 1;
    }
    paddingLength += (buffer.readUInt8(dataOffset) & 0xff);
    dataOffset += 1;
  } else if (frame.flags.PAD_HIGH) {
    return 'HEADERS frame got PAD_HIGH without PAD_LOW';
  }
  if (frame.flags.PRIORITY) {
    frame.priority = buffer.readUInt32BE(dataOffset) & 0x7fffffff;
    dataOffset += 4;
  }
  if (paddingLength) {
    frame.data = buffer.slice(dataOffset, -1 * paddingLength);
  } else {
    frame.data = buffer.slice(dataOffset);
  }
};

// [PRIORITY](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.3)
// -------------------------------------------------------
//
// The PRIORITY frame (type=0x2) specifies the sender-advised priority of a stream.
//
// The PRIORITY frame does not define any flags.

frameTypes[0x2] = 'PRIORITY';

frameFlags.PRIORITY = [];

typeSpecificAttributes.PRIORITY = ['priority'];

//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |X|                   Priority (31)                             |
//     +-+-------------------------------------------------------------+
//
// The payload of a PRIORITY frame contains a single reserved bit and a 31-bit priority.

Serializer.PRIORITY = function writePriority(frame, buffers) {
  var buffer = new Buffer(4);
  buffer.writeUInt32BE(frame.priority, 0);
  buffers.push(buffer);
};

Deserializer.PRIORITY = function readPriority(buffer, frame) {
  frame.priority = buffer.readUInt32BE(0);
};

// [RST_STREAM](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.4)
// -----------------------------------------------------------
//
// The RST_STREAM frame (type=0x3) allows for abnormal termination of a stream.
//
// No type-flags are defined.

frameTypes[0x3] = 'RST_STREAM';

frameFlags.RST_STREAM = [];

typeSpecificAttributes.RST_STREAM = ['error'];

//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |                         Error Code (32)                       |
//     +---------------------------------------------------------------+
//
// The RST_STREAM frame contains a single unsigned, 32-bit integer identifying the error
// code (see Error Codes). The error code indicates why the stream is being terminated.

Serializer.RST_STREAM = function writeRstStream(frame, buffers) {
  var buffer = new Buffer(4);
  var code = errorCodes.indexOf(frame.error);
  assert((0 <= code) && (code <= 0xffffffff), code);
  buffer.writeUInt32BE(code, 0);
  buffers.push(buffer);
};

Deserializer.RST_STREAM = function readRstStream(buffer, frame) {
  frame.error = errorCodes[buffer.readUInt32BE(0)];
};

// [SETTINGS](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.5)
// -------------------------------------------------------
//
// The SETTINGS frame (type=0x4) conveys configuration parameters that affect how endpoints
// communicate.
//
// The SETTINGS frame defines the following flag:

// * ACK (0x1):
//   Bit 1 being set indicates that this frame acknowledges receipt and application of the peer's
//   SETTINGS frame.
frameTypes[0x4] = 'SETTINGS';

frameFlags.SETTINGS = ['ACK'];

typeSpecificAttributes.SETTINGS = ['settings'];

// The payload of a SETTINGS frame consists of zero or more settings. Each setting consists of an
// 8-bit identifier, and an unsigned 32-bit value.
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |  Identifier(8)  |                  Value (32)                 |
//     +-----------------+---------------------------------------------+
//     ...Value          |
//     +-----------------+
//
// Each setting in a SETTINGS frame replaces the existing value for that setting.  Settings are
// processed in the order in which they appear, and a receiver of a SETTINGS frame does not need to
// maintain any state other than the current value of settings.  Therefore, the value of a setting
// is the last value that is seen by a receiver. This permits the inclusion of the same settings
// multiple times in the same SETTINGS frame, though doing so does nothing other than waste
// connection capacity.

Serializer.SETTINGS = function writeSettings(frame, buffers) {
  var settings = [], settingsLeft = Object.keys(frame.settings);
  definedSettings.forEach(function(setting, id) {
    if (setting.name in frame.settings) {
      settingsLeft.splice(settingsLeft.indexOf(setting.name), 1);
      var value = frame.settings[setting.name];
      settings.push({ id: id, value: setting.flag ? Boolean(value) : value });
    }
  });
  assert(settingsLeft.length === 0, 'Unknown settings: ' + settingsLeft.join(', '));

  var buffer = new Buffer(settings.length * 5);
  for (var i = 0; i < settings.length; i++) {
    buffer.writeUInt8(settings[i].id & 0xff, i*5);
    buffer.writeUInt32BE(settings[i].value, i*5 + 1);
  }

  buffers.push(buffer);
};

Deserializer.SETTINGS = function readSettings(buffer, frame, role) {
  frame.settings = {};

  if (buffer.length % 5 !== 0) {
    return 'Invalid SETTINGS frame';
  }
  for (var i = 0; i < buffer.length / 5; i++) {
    var id = buffer.readUInt8(i*5) & 0xff;
    var setting = definedSettings[id];
    if (setting) {
      if (role == 'CLIENT' && setting.name == 'SETTINGS_ENABLE_PUSH') {
        return 'SETTINGS frame on client got SETTINGS_ENABLE_PUSH';
      }
      var value = buffer.readUInt32BE(i*5 + 1);
      frame.settings[setting.name] = setting.flag ? Boolean(value & 0x1) : value;
    } else {
      /* Unknown setting, protocol error */
      return 'SETTINGS frame got unknown setting type';
    }
  }
};

// The following settings are defined:
var definedSettings = [];

// * SETTINGS_HEADER_TABLE_SIZE (1):
//   Allows the sender to inform the remote endpoint of the size of the header compression table
//   used to decode header blocks.
definedSettings[1] = { name: 'SETTINGS_HEADER_TABLE_SIZE', flag: false };

// * SETTINGS_ENABLE_PUSH (2):
//   This setting can be use to disable server push. An endpoint MUST NOT send a PUSH_PROMISE frame
//   if it receives this setting set to a value of 0. The default value is 1, which indicates that
//   push is permitted.
definedSettings[2] = { name: 'SETTINGS_ENABLE_PUSH', flag: true };

// * SETTINGS_MAX_CONCURRENT_STREAMS (4):
//   indicates the maximum number of concurrent streams that the sender will allow.
definedSettings[3] = { name: 'SETTINGS_MAX_CONCURRENT_STREAMS', flag: false };

// * SETTINGS_INITIAL_WINDOW_SIZE (7):
//   indicates the sender's initial stream window size (in bytes) for new streams.
definedSettings[4] = { name: 'SETTINGS_INITIAL_WINDOW_SIZE', flag: false };

// [PUSH_PROMISE](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.6)
// ---------------------------------------------------------------
//
// The PUSH_PROMISE frame (type=0x5) is used to notify the peer endpoint in advance of streams the
// sender intends to initiate.
//
// The PUSH_PROMISE frame defines the following flags:
//
// * END_PUSH_PROMISE (0x4):
//   The END_PUSH_PROMISE bit indicates that this frame contains the entire payload necessary to
//   provide a complete set of headers.

frameTypes[0x5] = 'PUSH_PROMISE';

frameFlags.PUSH_PROMISE = ['RESERVED1', 'RESERVED2', 'END_PUSH_PROMISE'];

typeSpecificAttributes.PUSH_PROMISE = ['promised_stream', 'headers', 'data'];

//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |X|                Promised-Stream-ID (31)                      |
//     +-+-------------------------------------------------------------+
//     |                    Header Block (*)                         ...
//     +---------------------------------------------------------------+
//
// The PUSH_PROMISE frame includes the unsigned 31-bit identifier of
// the stream the endpoint plans to create along with a minimal set of headers that provide
// additional context for the stream.

Serializer.PUSH_PROMISE = function writePushPromise(frame, buffers) {
  var buffer = new Buffer(4);

  var promised_stream = frame.promised_stream;
  assert((0 <= promised_stream) && (promised_stream <= 0x7fffffff), promised_stream);
  buffer.writeUInt32BE(promised_stream, 0);

  buffers.push(buffer);
  buffers.push(frame.data);
};

Deserializer.PUSH_PROMISE = function readPushPromise(buffer, frame) {
  frame.promised_stream = buffer.readUInt32BE(0) & 0x7fffffff;
  frame.data = buffer.slice(4);
};

// [PING](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.7)
// -----------------------------------------------
//
// The PING frame (type=0x6) is a mechanism for measuring a minimal round-trip time from the
// sender, as well as determining whether an idle connection is still functional.
//
// The PING frame defines one type-specific flag:
//
// * ACK (0x1):
//   Bit 1 being set indicates that this PING frame is a PING response.

frameTypes[0x6] = 'PING';

frameFlags.PING = ['ACK'];

typeSpecificAttributes.PING = ['data'];

// In addition to the frame header, PING frames MUST contain 8 additional octets of opaque data.

Serializer.PING = function writePing(frame, buffers) {
  buffers.push(frame.data);
};

Deserializer.PING = function readPing(buffer, frame) {
  if (buffer.length !== 8) {
    return 'Invalid size PING frame';
  }
  frame.data = buffer;
};

// [GOAWAY](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.8)
// ---------------------------------------------------
//
// The GOAWAY frame (type=0x7) informs the remote peer to stop creating streams on this connection.
//
// The GOAWAY frame does not define any flags.

frameTypes[0x7] = 'GOAWAY';

frameFlags.GOAWAY = [];

typeSpecificAttributes.GOAWAY = ['last_stream', 'error'];

//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |X|                  Last-Stream-ID (31)                        |
//     +-+-------------------------------------------------------------+
//     |                      Error Code (32)                          |
//     +---------------------------------------------------------------+
//
// The last stream identifier in the GOAWAY frame contains the highest numbered stream identifier
// for which the sender of the GOAWAY frame has received frames on and might have taken some action
// on.
//
// The GOAWAY frame also contains a 32-bit error code (see Error Codes) that contains the reason for
// closing the connection.

Serializer.GOAWAY = function writeGoaway(frame, buffers) {
  var buffer = new Buffer(8);

  var last_stream = frame.last_stream;
  assert((0 <= last_stream) && (last_stream <= 0x7fffffff), last_stream);
  buffer.writeUInt32BE(last_stream, 0);

  var code = errorCodes.indexOf(frame.error);
  assert((0 <= code) && (code <= 0xffffffff), code);
  buffer.writeUInt32BE(code, 4);

  buffers.push(buffer);
};

Deserializer.GOAWAY = function readGoaway(buffer, frame) {
  frame.last_stream = buffer.readUInt32BE(0) & 0x7fffffff;
  frame.error = errorCodes[buffer.readUInt32BE(4)];
};

// [WINDOW_UPDATE](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.9)
// -----------------------------------------------------------------
//
// The WINDOW_UPDATE frame (type=0x8) is used to implement flow control.
//
// The WINDOW_UPDATE frame does not define any flags.

frameTypes[0x8] = 'WINDOW_UPDATE';

frameFlags.WINDOW_UPDATE = [];

typeSpecificAttributes.WINDOW_UPDATE = ['window_size'];

// The payload of a WINDOW_UPDATE frame is a 32-bit value indicating the additional number of bytes
// that the sender can transmit in addition to the existing flow control window. The legal range
// for this field is 1 to 2^31 - 1 (0x7fffffff) bytes; the most significant bit of this value is
// reserved.

Serializer.WINDOW_UPDATE = function writeWindowUpdate(frame, buffers) {
  var buffer = new Buffer(4);

  var window_size = frame.window_size;
  assert((0 <= window_size) && (window_size <= 0x7fffffff), window_size);
  buffer.writeUInt32BE(window_size, 0);

  buffers.push(buffer);
};

Deserializer.WINDOW_UPDATE = function readWindowUpdate(buffer, frame) {
  frame.window_size = buffer.readUInt32BE(0) & 0x7fffffff;
};

// [CONTINUATION](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-6.10)
// ------------------------------------------------------------
//
// The CONTINUATION frame (type=0xA) is used to continue a sequence of header block fragments.
//
// The CONTINUATION frame defines the following flag:
//
// * END_HEADERS (0x4):
//   The END_HEADERS bit indicates that this frame ends the sequence of header block fragments
//   necessary to provide a complete set of headers.
// * PAD_LOW (0x10):
//   Bit 5 being set indicates that the Pad Low field is present.
// * PAD_HIGH (0x20):
//   Bit 6 being set indicates that the Pad High field is present. This bit MUST NOT be set unless
//   the PAD_LOW flag is also set. Endpoints that receive a frame with PAD_HIGH set and PAD_LOW
//   cleared MUST treat this as a connection error of type PROTOCOL_ERROR.

frameTypes[0x9] = 'CONTINUATION';

frameFlags.CONTINUATION = ['RESERVED1', 'RESERVED2', 'END_HEADERS', 'RESERVED8', 'PAD_LOW', 'PAD_HIGH'];

typeSpecificAttributes.CONTINUATION = ['headers', 'data'];

Serializer.CONTINUATION = function writeContinuation(frame, buffers) {
  buffers.push(frame.data);
};

Deserializer.CONTINUATION = function readContinuation(buffer, frame) {
  var dataOffset = 0;
  var paddingLength = 0;
  if (frame.flags.PAD_LOW) {
    if (frame.flags.PAD_HIGH) {
      paddingLength = (buffer.readUInt8(dataOffset) & 0xff) * 256;
      dataOffset += 1;
    }
    paddingLength += (buffer.readUInt8(dataOffset) & 0xff);
    dataOffset += 1;
  } else if (frame.flags.PAD_HIGH) {
    return 'CONTINUATION frame got PAD_HIGH without PAD_LOW';
  }
  if (paddingLength) {
    frame.data = buffer.slice(dataOffset, -1 * paddingLength);
  } else {
    frame.data = buffer.slice(dataOffset);
  }
};

// [Error Codes](http://tools.ietf.org/html/draft-ietf-httpbis-http2-10#section-7)
// ------------------------------------------------------------

var errorCodes = [
  'NO_ERROR',
  'PROTOCOL_ERROR',
  'INTERNAL_ERROR',
  'FLOW_CONTROL_ERROR',
  'SETTINGS_TIMEOUT',
  'STREAM_CLOSED',
  'FRAME_SIZE_ERROR',
  'REFUSED_STREAM',
  'CANCEL',
  'COMPRESSION_ERROR',
  'CONNECT_ERROR'
];
errorCodes[420] = 'ENHANCE_YOUR_CALM';

// Logging
// -------

// [Bunyan serializers](https://github.com/trentm/node-bunyan#serializers) to improve logging output
// for debug messages emitted in this component.
exports.serializers = {};

// * `frame` serializer: it transforms data attributes from Buffers to hex strings and filters out
//   flags that are not present.
var frameCounter = 0;
exports.serializers.frame = function(frame) {
  if (!frame) {
    return null;
  }

  if ('id' in frame) {
    return frame.id;
  }

  frame.id = frameCounter;
  frameCounter += 1;

  var logEntry = { id: frame.id };
  genericAttributes.concat(typeSpecificAttributes[frame.type]).forEach(function(name) {
    logEntry[name] = frame[name];
  });

  if (frame.data instanceof Buffer) {
    if (logEntry.data.length > 50) {
      logEntry.data = frame.data.slice(0, 47).toString('hex') + '...';
    } else {
      logEntry.data = frame.data.toString('hex');
    }

    if (!('length' in logEntry)) {
      logEntry.length = frame.data.length;
    }
  }

  if (frame.promised_stream instanceof Object) {
    logEntry.promised_stream = 'stream-' + frame.promised_stream.id;
  }

  logEntry.flags = Object.keys(frame.flags || {}).filter(function(name) {
    return frame.flags[name] === true;
  });

  return logEntry;
};

// * `data` serializer: it simply transforms a buffer to a hex string.
exports.serializers.data = function(data) {
  return data.toString('hex');
};
