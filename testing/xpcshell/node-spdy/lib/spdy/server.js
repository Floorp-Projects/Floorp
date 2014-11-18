var spdy = require('../spdy'),
    util = require('util'),
    https = require('https');

var Connection = spdy.Connection;

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
    // Initialize
    this._init(HTTPSServer, options, requestListener);

    // Wrap connection handler
    this._wrap();
  };
  util.inherits(Server, HTTPSServer);

  // Copy prototype methods
  Object.keys(proto).forEach(function(key) {
    this[key] = proto[key];
  }, Server.prototype);

  return Server;
}
exports.instantiate = instantiate;

// Common prototype for all servers
var proto = {};

//
// ### function _init(base, options, listener)
// #### @base {Function} (optional) base server class (https.Server)
// #### @options {Object} tls server options
// #### @handler {Function} (optional) request handler
// Initializer.
//
proto._init = function _init(base, options, handler) {
  var state = {};
  this._spdyState = state;

  if (!options)
    options = {};

  // Copy user supplied options
  options = util._extend({}, options);

  var supportedProtocols = [
    'spdy/3.1', 'spdy/3', 'spdy/2',
    'http/1.1', 'http/1.0'
  ];
  options.NPNProtocols = supportedProtocols;
  options.ALPNProtocols = supportedProtocols;
  options.isServer = true;

  // Header compression is enabled by default in servers, in contrast to clients
  // where it is disabled to prevent CRIME attacks.
  // See: https://groups.google.com/d/msg/spdy-dev/6mVYRv-lbuc/qGcW2ldOpt8J
  if (options.headerCompression !== false)
    options.headerCompression = true;

  state.options = options;
  state.reqHandler = handler;

  if (options.plain && !options.ssl)
    base.call(this, handler);
  else
    base.call(this, options, handler);

  // Use https if neither NPN or ALPN is supported
  if (!process.features.tls_npn && !process.features.tls_alpn &&
      !options.debug && !options.plain)
    return;
};

//
// ### function _wrap()
// Wrap connection handler and add logic.
//
proto._wrap = function _wrap() {
  var self = this,
      state = this._spdyState;

  // Wrap connection handler
  var event = state.options.plain && !state.options.ssl ? 'connection' :
                                                          'secureConnection',
      handler = this.listeners(event)[0];

  state.handler = handler;

  this.removeAllListeners(event);

  // 2 minutes default timeout
  if (state.options.timeout !== undefined)
    this.timeout = state.options.timeout;
  else
    this.timeout = this.timeout || 2 * 60 * 1000;

  // Normal mode, use NPN or ALPN to fallback to HTTPS
  if (!state.options.plain)
    return this.on(event, this._onConnection.bind(this));

  // In case of plain connection, we must fallback to HTTPS if first byte
  // is not equal to 0x80.
  this.on(event, function(socket) {
    var history = [],
        _emit = socket.emit;

    // Add 'data' listener, otherwise 'data' events won't be emitted
    if (spdy.utils.isLegacy) {
      function ondata() {};
      socket.once('data', ondata);
    }

    // 2 minutes timeout, as http.js does by default
    socket.setTimeout(self.timeout);

    socket.emit = function emit(event, data) {
      history.push(Array.prototype.slice.call(arguments));

      if (event === 'data') {
        // Legacy
        if (spdy.utils.isLegacy)
          onFirstByte.call(socket, data);
      } else if (event === 'readable') {
        // Streams
        onReadable.call(socket);
      } else if (event === 'end' ||
                 event === 'close' ||
                 event === 'error' ||
                 event === 'timeout') {
        // We shouldn't get there if any data was received
        fail();
      }
    };

    function fail() {
      socket.emit = _emit;
      history = null;
      socket.removeListener('readable', onReadable);

      try {
        socket.destroy();
      } catch (e) {
      }
    }

    function restore() {
      var copy = history.slice();
      history = null;

      socket.removeListener('readable', onReadable);
      if (spdy.utils.isLegacy)
        socket.removeListener('data', ondata);
      socket.emit = _emit;
      for (var i = 0; i < copy.length; i++) {
        if (copy[i][0] !== 'data' || spdy.utils.isLegacy)
          socket.emit.apply(socket, copy[i]);
        if (copy[i][0] === 'end' && socket.onend)
          socket.onend();
      }
    }

    function onFirstByte(data) {
      // Ignore empty packets
      if (data.length === 0)
        return;

      if (data[0] === 0x80)
        self._onConnection(socket);
      else
        handler.call(self, socket);

      // Fire events
      restore();

      // NOTE: If we came there - .ondata() will be called anyway in this tick,
      // so there're no need to call it manually
    };

    // Hack to make streams2 work properly
    if (!spdy.utils.isLegacy)
      socket.on('readable', onReadable);

    function onReadable() {
      var data = socket.read(1);

      // Ignore empty packets
      if (!data)
        return;
      socket.removeListener('readable', onReadable);

      // `.unshift()` emits `readable` event. Thus `emit` method should
      // be restored before calling it.
      socket.emit = _emit;

      // Put packet back where it was before
      socket.unshift(data);

      if (data[0] === 0x80)
        self._onConnection(socket);
      else
        handler.call(self, socket);

      // Fire events
      restore();

      if (socket.ondata) {
        data = socket.read(socket._readableState.length);
        if (data)
          socket.ondata(data, 0, data.length);
      }
    }
  });
};

//
// ### function _onConnection (socket)
// #### @socket {Stream} incoming socket
// Server's connection handler wrapper.
//
proto._onConnection = function _onConnection(socket) {
  var self = this,
      state = this._spdyState;

  // Fallback to HTTPS if needed
  var selectedProtocol = socket.npnProtocol || socket.alpnProtocol;
  if ((!selectedProtocol || !selectedProtocol.match(/spdy/)) &&
      !state.options.debug && !state.options.plain) {
    return state.handler.call(this, socket);
  }

  // Wrap incoming socket into abstract class
  var connection = new Connection(socket, state.options, this);
  if (selectedProtocol === 'spdy/3.1')
    connection._setVersion(3.1);
  else if (selectedProtocol === 'spdy/3')
    connection._setVersion(3);
  else if (selectedProtocol === 'spdy/2')
    connection._setVersion(2);

  // Emulate each stream like connection
  connection.on('stream', state.handler);

  connection.on('connect', function onconnect(req, socket) {
    socket.streamID = req.streamID = req.socket._spdyState.id;
    socket.isSpdy = req.isSpdy = true;
    socket.spdyVersion = req.spdyVersion = req.socket._spdyState.framer.version;

    socket.once('finish', function onfinish() {
      req.connection.end();
    });

    self.emit('connect', req, socket);
  });

  connection.on('request', function onrequest(req, res) {
    // Copy extension methods
    res._renderHeaders = spdy.response._renderHeaders;
    res.writeHead = spdy.response.writeHead;
    res.end = spdy.response.end;
    res.push = spdy.response.push;
    res.streamID = req.streamID = req.socket._spdyState.id;
    res.spdyVersion = req.spdyVersion = req.socket._spdyState.framer.version;
    res.isSpdy = req.isSpdy = true;
    res.addTrailers = function addTrailers(headers) {
      res.socket.sendHeaders(headers);
    };

    // Make sure that keep-alive won't apply to the response
    res._last = true;

    // Chunked encoding is not supported in SPDY
    res.useChunkedEncodingByDefault = false;

    // Populate trailing headers
    req.connection.on('headers', function(headers) {
      Object.keys(headers).forEach(function(key) {
        req.trailers[key] = headers[key];
      });
      req.emit('trailers', headers);
    });

    self.emit('request', req, res);
  });

  connection.on('error', function onerror(e) {
    socket.destroy(e.errno === 'EPIPE' ? undefined : e);
  });
};

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

  // Instantiate http server if `ssl: false`
  if (!base && options && options.plain && options.ssl === false)
    return exports.create(require('http').Server, options, requestListener);

  return new server(options, requestListener);
};
