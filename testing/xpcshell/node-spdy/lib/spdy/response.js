var spdy = require('../spdy'),
    http = require('http'),
    res = http.ServerResponse.prototype;

//
// ### function _renderHeaders ()
// Copy pasted from lib/http.js
// (added lowercase)
//
exports._renderHeaders = function() {
  if (this._header) {
    throw new Error("Can't render headers after they are sent to the client.");
  }

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
exports.writeHead = function(statusCode) {
  if (this._headerSent) return;
  this._headerSent = true;

  var reasonPhrase, headers, headerIndex;

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
      if (k) headers[k] = obj[k];
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
  if (this.socket._isGoaway()) return;

  this.socket._lock(function() {
    var socket = this;

    this._framer.replyFrame(
      this.id,
      statusCode,
      reasonPhrase,
      headers,
      function (err, frame) {
        // TODO: Handle err
        socket.connection.write(frame);
        socket._unlock();
      }
    );
  });
};

//
// ### function push (url, headers, callback)
// #### @url {String} absolute or relative url (from root anyway)
// #### @headers {Object} response headers
// #### @callbacks {Function} continuation that will receive stream object
// Initiates push stream
//
exports.push = function push(url, headers, priority, callback) {
  if (this.socket._destroyed) {
    return callback(Error('Can\'t open push stream, parent socket destroyed'));
  }

  if (!callback && typeof priority === 'function') {
    callback = priority;
    priority = 0;
  }

  if (!callback) callback = function() {};

  this.socket._lock(function() {
    var socket = this,
        id = socket.connection.pushId += 2,
        scheme = this._frame.headers.scheme,
        host = this._frame.headers.host || 'localhost',
        fullUrl = /^\//.test(url) ? scheme + '://' + host + url : url;

    this._framer.streamFrame(
      id,
      this.id,
      {
        method: 'GET',
        path: url,
        url: fullUrl,
        scheme: scheme,
        host: host,
        version: 'HTTP/1.1',
        priority: priority || 0
      },
      headers,
      function(err, frame) {
        if (err) {
          socket._unlock();
          callback(err);
        } else {
          socket.connection.write(frame);
          socket._unlock();

          var stream = new spdy.server.Stream(socket.connection, {
            type: 'SYN_STREAM',
            push: true,
            id: id,
            assoc: socket.id,
            priority: 0,
            headers: {}
          });

          socket.connection.streams[id] = stream;
          socket.pushes.push(stream);

          callback(null, stream);
        }
      }
    );
  });
};
