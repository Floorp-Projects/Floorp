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
    version: 3,
    associated: stream ? data.readUInt32BE(4, true) & 0x7fffffff : 0,
    priority: stream ? data[8] >> 5 : 0,
    fin: (flags & 0x01) === 0x01,
    unidir: (flags & 0x02) === 0x02,
    _offset: stream ? 10 : 4
  };
};

//
// ### function parseHeaders (pairs)
// #### @pairs {Buffer} header pairs
// Returns hashmap of parsed headers
//
protocol.parseHeaders = function parseHeaders(pairs) {
  var count = pairs.readUInt32BE(0, true),
      headers = {};

  pairs = pairs.slice(4);

  function readString() {
    var len = pairs.readUInt32BE(0, true),
        value = pairs.slice(4, 4 + len);

    pairs = pairs.slice(4 + len);

    return value.toString();
  }

  while(count > 0) {
    headers[readString().replace(/^:/, '')] = readString();
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

protocol.parseSettings = function parseSettings(data) {
  var settings = {},
      number = data.readUInt32BE(0, true),
      idMap = {
        1: 'upload_bandwidth',
        2: 'download_bandwidth',
        3: 'round_trip_time',
        4: 'max_concurrent_streams',
        5: 'current_cwnd',
        6: 'download_retrans_rate',
        7: 'initial_window_size',
        8: 'client_certificate_vector_size'
      };

  for (var i=0; i<number; i++) {
    var id = data.readUInt32BE(4 + (i*8), true) & 0x00ffffff,
        flags = data.readUInt8(4 + (i*8), true),
        name = idMap[id];
    settings[id] = settings[name] = {
      persist: !!(flags & 0x1),
      persisted: !!(flags & 0x2),
      value: data.readUInt32BE(8 + (i*8), true)
    };
  }

  return {
    type: 'SETTINGS',
    settings: settings
  };
};

protocol.parseGoaway = function parseGoaway(data) {
  return {
    type: 'GOAWAY',
    lastId: data.readUInt32BE(0, true) & 0x7fffffff
  };
};

protocol.parseWindowUpdate = function parseWindowUpdate(data) {
  var ret = {
    type: 'WINDOW_UPDATE',
    id: data.readUInt32BE(0, true) & 0x7fffffff,
    delta: data.readUInt32BE(4, true) & 0x7fffffff
  };

  return ret;
};
