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

var Promise = require('../util/promise').Promise;

/**
 * This is how to implement a connector
 *  var baseConnector = {
 *    item: 'connector',
 *    name: 'foo',
 *
 *    connect: function(url) {
 *      return Promise.resolve(new FooConnection(url));
 *    }
 *  };
 */

/**
 * A prototype base for Connectors
 */
function Connection() {
}

/**
 * Add an event listener
 */
Connection.prototype.on = function(event, action) {
  if (!this._listeners) {
    this._listeners = {};
  }
  if (!this._listeners[event]) {
    this._listeners[event] = [];
  }
  this._listeners[event].push(action);
};

/**
 * Remove an event listener
 */
Connection.prototype.off = function(event, action) {
  if (!this._listeners) {
    return;
  }
  var actions = this._listeners[event];
  if (actions) {
    this._listeners[event] = actions.filter(function(li) {
      return li !== action;
    }.bind(this));
  }
};

/**
 * Emit an event. For internal use only
 */
Connection.prototype._emit = function(event, data) {
  if (this._listeners == null || this._listeners[event] == null) {
    return;
  }

  var listeners = this._listeners[event];
  listeners.forEach(function(listener) {
    // Fail fast if we mutate the list of listeners while emitting
    if (listeners !== this._listeners[event]) {
      throw new Error('Listener list changed while emitting');
    }

    try {
      listener.call(null, data);
    }
    catch (ex) {
      console.log('Error calling listeners to ' + event);
      console.error(ex);
    }
  }.bind(this));
};

/**
 * Send a message to the other side of the connection
 */
Connection.prototype.call = function(feature, data) {
  throw new Error('Not implemented');
};

/**
 * Disconnecting a Connection destroys the resources it holds. There is no
 * common route back to being connected once this has been called
 */
Connection.prototype.disconnect = function() {
  return Promise.resolve();
};

exports.Connection = Connection;

/**
 * A manager for the registered Connectors
 */
function Connectors() {
  // This is where we cache the connectors that we know about
  this._registered = {};
}

/**
 * Add a new connector to the cache
 */
Connectors.prototype.add = function(connector) {
  this._registered[connector.name] = connector;
};

/**
 * Remove an existing connector from the cache
 */
Connectors.prototype.remove = function(connector) {
  var name = typeof connector === 'string' ? connector : connector.name;
  delete this._registered[name];
};

/**
 * Get access to the list of known connectors
 */
Connectors.prototype.getAll = function() {
  return Object.keys(this._registered).map(function(name) {
    return this._registered[name];
  }.bind(this));
};

/**
 * Get access to a connector by name. If name is undefined then use the first
 * registered connector as a default.
 */
Connectors.prototype.get = function(name) {
  if (name == null) {
    name = Object.keys(this._registered)[0];
  }
  return this._registered[name];
};

exports.Connectors = Connectors;
