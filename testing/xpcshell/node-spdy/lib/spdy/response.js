var spdy = require('../spdy'),
    utils = spdy.utils,
    http = require('http'),
    Stream = require('stream').Stream,
    res = http.ServerResponse.prototype;

//
// ### function _renderHeaders ()
// Copy pasted from lib/http.js
// (added lowercase)
//
exports._renderHeaders = function renderHeaders() {
  if (this._header)
    throw new Error("Can't render headers after they are sent to the client.");

  var keys = Object.keys(this._headerNames);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    this._headerNames[key] = this._headerNames[key].toLowerCase();
  }

  return res._renderHeaders.call(this);
};

//
// ### function writeHead (statusCode)
// #### @statusCode {Number} HTTP Status code
// .writeHead() wrapper
// (Sorry, copy pasted from lib/http.js)
//
exports.writeHead = function writeHead(statusCode) {
  if (this._headerSent)
    return;
  this._headerSent = true;

  var reasonPhrase, headers = {}, headerIndex;

  if (typeof arguments[1] == 'string') {
    reasonPhrase = arguments[1];
    headerIndex = 2;
  } else {
    reasonPhrase = http.STATUS_CODES[statusCode] || 'unknown';
    headerIndex = 1;
  }
  this.statusCode = statusCode;

  var obj = arguments[headerIndex];

  if (obj && this._headers) {
    // Slow-case: when progressive API and header fields are passed.
    headers = this._renderHeaders();

    // handle object case
    var keys = Object.keys(obj);
    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      if (k)
        headers[k] = obj[k];
    }
  } else if (this._headers) {
    // only progressive api is used
    headers = this._renderHeaders();
  } else {
    // only writeHead() called
    headers = obj;
  }

  // cleanup
  this._header = '';

  // Do not send data to new connections after GOAWAY
  if (this.socket._isGoaway())
    return;

  // Date header
  if (this.sendDate === true) {
    if (headers === undefined)
      headers = {};
    if (headers.date === undefined)
      headers.date = new Date().toUTCString();
  }

  this.socket._lock(function() {
    var socket = this;

    this._spdyState.framer.replyFrame(
      this._spdyState.id,
      statusCode,
      reasonPhrase,
      headers,
      function (err, frame) {
        if (err) {
          socket._unlock();
          socket.emit('error', err);
          return;
        }

        socket.connection.cork();
        socket.connection.write(frame);
        utils.nextTick(function() {
          socket.connection.uncork();
        });
        socket._unlock();
      }
    );
  });
};

//
// ### function end (data, encoding, cb)
// #### @data {Buffer|String} (optional) data
// #### @encoding {String} (optional) string encoding
// #### @cb {Function}
// Send final data
//
exports.end = function end(data, encoding, cb) {
  if (this.socket)
    this.socket._spdyState.ending = true;

  this.constructor.prototype.end.call(this, data, encoding, cb);
};

//
// ### function push (url, headers, callback)
// #### @url {String} absolute or relative url (from root anyway)
// #### @headers {Object} response headers
// #### @callbacks {Function} continuation that will receive stream object
// Initiates push stream
//
exports.push = function push(url, headers, priority, callback) {
  var socket = this.socket;

  if (!callback && typeof priority === 'function') {
    callback = priority;
    priority = null;
  }
  if (!priority && typeof priority !== 'number')
    priority = 7;

  if (!callback)
    callback = function() {};

  if (!socket || socket._destroyed) {
    var stub = new Stream();
    var err = Error('Can\'t open push stream, parent socket destroyed');
    utils.nextTick(function() {
      if (stub.listeners('error').length !== 0)
        stub.emit('error', err);
      callback(err);
    });
    return stub;
  }

  var id = socket.connection._spdyState.pushId += 2,
      scheme = socket._spdyState.scheme,
      host = headers.host || socket._spdyState.host || 'localhost',
      fullUrl = /^\//.test(url) ? scheme + '://' + host + url : url;

  var stream = new spdy.Stream(socket.connection, {
    type: 'SYN_STREAM',
    id: id,
    associated: socket._spdyState.id,
    priority: priority,
    headers: {}
  });

  stream.associated = socket;
  socket.connection._addStream(stream);

  socket._lock(function() {
    this._spdyState.framer.streamFrame(
      id,
      this._spdyState.id,
      {
        method: 'GET',
        path: url,
        url: fullUrl,
        scheme: scheme,
        host: host,
        version: 'HTTP/1.1',
        priority: priority,
        status: 200
      },
      headers,
      function(err, frame) {
        if (err) {
          socket._unlock();
          if (callback)
            callback(err);
          stream.destroy(err);
          return;
        } else {
          socket.connection.cork();
          socket.connection.write(frame);
          utils.nextTick(function() {
            socket.connection.uncork();
          });
          socket._unlock();
        }

        stream.emit('acknowledge');
        if (callback)
          callback(null, stream);
      }
    );
  });

  return stream;
};
