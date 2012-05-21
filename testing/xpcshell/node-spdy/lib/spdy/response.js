/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var spdy = require('../spdy'),
    http = require('http');

//
// ### function _renderHeaders ()
// Copy pasted from lib/http.js
// (added lowercase)
//
exports._renderHeaders = function() {
  if (this._header) {
    throw new Error("Can't render headers after they are sent to the client.");
  }

  if (!this._headers) return {};

  var headers = {};
  var keys = Object.keys(this._headers);
  for (var i = 0, l = keys.length; i < l; i++) {
    var key = keys[i];
    headers[(this._headerNames[key] || '').toLowerCase()] = this._headers[key];
  }
  return headers;
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
  if (this.socket.isGoaway()) return;

  this.socket.lock(function() {
    var socket = this;

    this.framer.replyFrame(
      this.id,
      statusCode,
      reasonPhrase,
      headers,
      function (err, frame) {
        // TODO: Handle err
        socket.connection.write(frame);
        socket.unlock();
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
exports.push = function push(url, headers, callback) {
  if (this.socket._destroyed) {
    return callback(Error('Can\'t open push stream, parent socket destroyed'));
  }

  this.socket.lock(function() {
    var socket = this,
        id = socket.connection.pushId += 2,
        fullUrl = /^\//.test(url) ?
                      this.frame.headers.scheme + '://' +
                      (this.frame.headers.host || 'localhost') +
                      url
                      :
                      url;

    this.framer.streamFrame(
      id,
      this.id,
      {
        method: 'GET',
        url: fullUrl,
        schema: 'https',
        version: 'HTTP/1.1'
      },
      headers,
      function(err, frame) {
        if (err) {
          socket.unlock();
          callback(err);
        } else {
          socket.connection.write(frame);
          socket.unlock();

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
