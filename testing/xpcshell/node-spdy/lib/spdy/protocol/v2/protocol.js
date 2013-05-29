var protocol = exports;

//
// ### function parseSynHead (type, flags, data)
// #### @type {Number} Frame type
// #### @flags {Number} Frame flags
// #### @data {Buffer} input data
// Returns parsed syn_* frame's head
//
protocol.parseSynHead = function parseSynHead(type, flags, data) {
  var stream = type === 0x01;

  return {
    type: stream ? 'SYN_STREAM' : 'SYN_REPLY',
    id: data.readUInt32BE(0, true) & 0x7fffffff,
    version: 2,
    associated: stream ? data.readUInt32BE(4, true) & 0x7fffffff : 0,
    priority: stream ? data[8] >> 6 : 0,
    fin: (flags & 0x01) === 0x01,
    unidir: (flags & 0x02) === 0x02,
    _offset: stream ? 10 : 6
  };
};

//
// ### function parseHeaders (pairs)
// #### @pairs {Buffer} header pairs
// Returns hashmap of parsed headers
//
protocol.parseHeaders = function parseHeaders(pairs) {
  var count = pairs.readUInt16BE(0, true),
      headers = {};

  pairs = pairs.slice(2);

  function readString() {
    var len = pairs.readUInt16BE(0, true),
        value = pairs.slice(2, 2 + len);

    pairs = pairs.slice(2 + len);

    return value.toString();
  }

  while(count > 0) {
    headers[readString()] = readString();
    count--;
  }

  return headers;
};

//
// ### function parsesRst frame
protocol.parseRst = function parseRst(data) {
  return {
    type: 'RST_STREAM',
    id: data.readUInt32BE(0, true) & 0x7fffffff,
    status: data.readUInt32BE(4, true)
  };
};

protocol.parseGoaway = function parseGoaway(data) {
  return {
    type: 'GOAWAY',
    lastId: data.readUInt32BE(0, true) & 0x7fffffff
  };
};
