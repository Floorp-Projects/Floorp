/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var Cu = require('chrome').Cu;

var debuggerSocketConnect = Cu.import('resource://gre/modules/devtools/dbg-client.jsm', {}).debuggerSocketConnect;
var DebuggerClient = Cu.import('resource://gre/modules/devtools/dbg-client.jsm', {}).DebuggerClient;

var Promise = require('../util/promise').Promise;
var Connection = require('./connectors').Connection;

/**
 * What port should we use by default?
 */
Object.defineProperty(exports, 'defaultPort', {
  get: function() {
    var Services = Cu.import('resource://gre/modules/Services.jsm', {}).Services;
    try {
      return Services.prefs.getIntPref('devtools.debugger.chrome-debugging-port');
    }
    catch (ex) {
      console.error('Can\'t use default port from prefs. Using 9999');
      return 9999;
    }
  },
  enumerable: true
});

exports.items = [
  {
    item: 'connector',
    name: 'rdp',

    connect: function(url) {
      return RdpConnection.create(url);
    }
  }
];

/**
 * RdpConnection uses the Firefox Remote Debug Protocol
 */
function RdpConnection(url) {
  throw new Error('Use RdpConnection.create');
}

/**
 * Asynchronous construction
 */
RdpConnection.create = function(url) {
  this.host = url;
  this.port = undefined; // TODO: Split out the port number

  this.requests = {};
  this.nextRequestId = 0;

  this._emit = this._emit.bind(this);

  return new Promise(function(resolve, reject) {
    this.transport = debuggerSocketConnect(this.host, this.port);
    this.client = new DebuggerClient(this.transport);
    this.client.connect(function() {
      this.client.listTabs(function(response) {
        this.actor = response.gcliActor;
        resolve();
      }.bind(this));
    }.bind(this));
  }.bind(this));
};

RdpConnection.prototype = Object.create(Connection.prototype);

RdpConnection.prototype.call = function(command, data) {
  return new Promise(function(resolve, reject) {
    var request = { to: this.actor, type: command, data: data };

    this.client.request(request, function(response) {
      resolve(response.commandSpecs);
    });
  }.bind(this));
};

RdpConnection.prototype.disconnect = function() {
  return new Promise(function(resolve, reject) {
    this.client.close(function() {
      resolve();
    });

    delete this._emit;
  }.bind(this));
};


/**
 * A Request is a command typed at the client which lives until the command
 * has finished executing on the server
 */
function Request(actor, typed, args) {
  this.json = {
    to: actor,
    type: 'execute',
    typed: typed,
    args: args,
    requestId: 'id-' + Request._nextRequestId++,
  };

  this.promise = new Promise(function(resolve, reject) {
    this._resolve = resolve;
  }.bind(this));
}

Request._nextRequestId = 0;

/**
 * Called by the connection when a remote command has finished executing
 * @param error boolean indicating output state
 * @param type the type of the returned data
 * @param data the data itself
 */
Request.prototype.complete = function(error, type, data) {
  this._resolve({
    error: error,
    type: type,
    data: data
  });
};
