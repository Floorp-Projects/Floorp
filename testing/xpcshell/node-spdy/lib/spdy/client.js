var spdy = require('../spdy');
var assert = require('assert');
var util = require('util');
var net = require('net');
var https = require('https');
var EventEmitter = require('events').EventEmitter;

var proto = {};

function instantiate(base) {
  function Agent(options) {
    base.call(this, options);

    if (!this.options.spdy)
      this.options.spdy = {};

    this._init();

    // Hack for node.js v0.10
    this.createConnection = proto.createConnection;

    // No chunked encoding
    this.keepAlive = false;
  };
  util.inherits(Agent, base);

  // Copy prototype methods
  Object.keys(proto).forEach(function(key) {
    this[key] = proto[key];
  }, Agent.prototype);

  return Agent;
};

proto._init = function init() {
  var self = this;

  var state = {};
  // Find super's `createConnection` method
  var createConnection;
  var cons = this.constructor.super_;
  do {
    createConnection = cons.prototype.createConnection;

    if (cons.super_ === EventEmitter || !cons.super_)
      break;
    cons = cons.super_;
  } while (!createConnection);

  if (!createConnection)
    createConnection = this.createConnection || net.createConnection;

  // TODO(indutny): Think about falling back to http/https
  var socket = createConnection.call(this, util._extend({
    NPNProtocols: ['spdy/3.1', 'spdy/3', 'spdy/2'],
    ALPNProtocols: ['spdy/3.1', 'spdy/3', 'spdy/2']
  }, this.options));
  state.socket = socket;

  // Header compression is disabled by default in clients to prevent CRIME
  // attacks. Don't enable it unless you know what you're doing.
  if (this.options.spdy.headerCompression !== true)
    this.options.spdy.headerCompression = false;

  // Create SPDY connection using newly created socket
  var connection = new spdy.Connection(socket,
                                       util._extend(this.options.spdy, {
    isServer: false
  }));
  state.connection = connection;

  connection.on('error', function(err) {
    self.emit('error', err);
  });

  // Select SPDY version
  if (this.options.spdy.ssl !== false && !this.options.spdy.version) {
    // SSL, wait for NPN or ALPN to happen
    socket.once('secureConnect', function() {
      var selectedProtocol = socket.npnProtocol || socket.alpnProtocol;
      if (selectedProtocol === 'spdy/2')
        connection._setVersion(2);
      else if (selectedProtocol === 'spdy/3')
        connection._setVersion(3);
      else if (selectedProtocol === 'spdy/3.1')
        connection._setVersion(3.1);
      else
        socket.emit('error', new Error('No supported SPDY version'));
    });
  } else {
    // No SSL, use fixed version
    connection._setVersion(this.options.spdy.version || 3);
  }

  // Destroy PUSH streams until the listener will be added
  connection.on('stream', function(stream) {
    stream.on('error', function() {});
    stream.destroy();
  });
  state.pushServer = null;

  this.on('newListener', this._onNewListener.bind(this));
  this.on('removeListener', this._onRemoveListener.bind(this));

  // Client has odd-numbered stream ids
  state.id = 1;
  this._spdyState = state;
};

//
// ### function onNewListener (type, listener)
// #### @type {String} Event type
// #### @listener {Function} Listener callback
//
proto._onNewListener = function onNewListener(type, listener) {
  if (type !== 'push')
    return;

  var state = this._spdyState;
  if (state.pushServer)
    return state.pushServer.on('translated-request', listener);

  state.pushServer = require('http').createServer(listener);
  state.connection.removeAllListeners('stream');
  state.connection.on('stream', function(stream) {
    state.pushServer.emit('connection', stream);
  });

  state.pushServer.on('request', function(req) {
    // Populate trailing headers
    req.connection.on('headers', function(headers) {
      Object.keys(headers).forEach(function(key) {
        req.trailers[key] = headers[key];
      });
      req.emit('trailers', headers);
    });

    this.emit('translated-request', req);
  });
};

//
// ### function onRemoveListener (type, listener)
// #### @type {String} Event type
// #### @listener {Function} Listener callback
//
proto._onRemoveListener = function onRemoveListener(type, listener) {
  if (type !== 'push')
    return;

  var state = this._spdyState;
  if (!state.pushServer)
    return;

  state.pushServer.removeListener('translated-request', listener);
};

//
// ### function createConnection (options)
//
proto.createConnection = function createConnection(options) {
  if (!options)
    options = {};

  var state = this._spdyState;
  var stream = new spdy.Stream(state.connection, {
    id: state.id,
    priority: options.priority || 7,
    client: true,
    decompress: options.spdy.decompress == undefined ? true :
                                                       options.spdy.decompress
  });
  state.id += 2;
  state.connection._addStream(stream);

  return stream;
};

//
// ### function close (callback)
// #### @callback {Function}
// Close underlying socket and terminate all streams
//
proto.close = function close(callback) {
  this._spdyState.socket.destroySoon();
  if (callback)
    this._spdyState.socket.once('close', callback);
};

//
// ### function ping (callback)
// #### @callback {Function}
// Send PING frame and invoke callback once received it back
//
proto.ping = function ping(callback) {
  return this._spdyState.connection.ping(callback);
};

//
// Default Agent
//
exports.Agent = instantiate(https.Agent);

//
// ### function create (base, options)
// #### @base {Function} (optional) base server class (https.Server)
// #### @options {Object} tls server options
// @constructor wrapper
//
exports.create = function create(base, options) {
  var agent;
  if (typeof base === 'function') {
    agent = instantiate(base);
  } else {
    agent = exports.Agent;

    options = base;
    base = null;
  }

  // Instantiate http server if `ssl: false`
  if (!base &&
      options &&
      options.spdy &&
      options.spdy.plain &&
      options.spdy.ssl === false) {
    return exports.create(require('http').Agent, options);
  }

  return new agent(options);
};
