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
var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;

var OS = Cu.import('resource://gre/modules/osfile.jsm', {}).OS;

var promise = require('./promise');
var util = require('./util');

function Highlighter(document) {
  this._document = document;
  this._nodes = util.createEmptyNodeList(this._document);
}

Object.defineProperty(Highlighter.prototype, 'nodelist', {
  set: function(nodes) {
    Array.prototype.forEach.call(this._nodes, this._unhighlightNode, this);
    this._nodes = (nodes == null) ?
        util.createEmptyNodeList(this._document) :
        nodes;
    Array.prototype.forEach.call(this._nodes, this._highlightNode, this);
  },
  get: function() {
    return this._nodes;
  },
  enumerable: true
});

Highlighter.prototype.destroy = function() {
  this.nodelist = null;
};

Highlighter.prototype._highlightNode = function(node) {
  // Enable when the highlighter rewrite is done
};

Highlighter.prototype._unhighlightNode = function(node) {
  // Enable when the highlighter rewrite is done
};

exports.Highlighter = Highlighter;

/**
 * See docs in lib/gcli/util/host.js:exec
 */
exports.exec = function(execSpec) {
  throw new Error('Not supported');
};

/**
 * Asynchronously load a text resource
 * @see lib/gcli/util/host.js
 */
exports.staticRequire = function(requistingModule, name) {
  var deferred = promise.defer();

  if (name.match(/\.css$/)) {
    deferred.resolve('');
  }
  else {
    var filename = OS.Path.dirname(requistingModule.id) + '/' + name;
    filename = filename.replace(/\/\.\//g, '/');
    filename = 'resource://gre/modules/devtools/' + filename;

    var xhr = Cc['@mozilla.org/xmlextras/xmlhttprequest;1']
                .createInstance(Ci.nsIXMLHttpRequest);

    xhr.onload = function onload() {
      deferred.resolve(xhr.responseText);
    }.bind(this);

    xhr.onabort = xhr.onerror = xhr.ontimeout = function(err) {
      deferred.reject(err);
    }.bind(this);

    try {
      xhr.open('GET', filename);
      xhr.send();
    }
    catch (ex) {
      deferred.reject(ex);
    }
  }

  return deferred.promise;
};

/**
 * A group of functions to help scripting. Small enough that it doesn't need
 * a separate module (it's basically a wrapper around 'eval' in some contexts)
 */
var client;
var target;
var consoleActor;
var webConsoleClient;

exports.script = { };

exports.script.onOutput = util.createEvent('Script.onOutput');

/**
 * Setup the environment to eval JavaScript
 */
exports.script.useTarget = function(tgt) {
  target = tgt;

  // Local debugging needs to make the target remote.
  var targetPromise = target.isRemote ?
                      promise.resolve(target) :
                      target.makeRemote();

  return targetPromise.then(function() {
    var deferred = promise.defer();

    client = target._client;

    client.addListener('pageError', function(packet) {
      if (packet.from === consoleActor) {
        // console.log('pageError', packet.pageError);
        exports.script.onOutput({
          level: 'exception',
          message: packet.exception.class
        });
      }
    });

    client.addListener('consoleAPICall', function(type, packet) {
      if (packet.from === consoleActor) {
        var data = packet.message;

        var ev = {
          level: data.level,
          arguments: data.arguments,
        };

        if (data.filename !== 'debugger eval code') {
          ev.source = {
            filename: data.filename,
            lineNumber: data.lineNumber,
            functionName: data.functionName
          };
        }

        exports.script.onOutput(ev);
      }
    });

    consoleActor = target._form.consoleActor;

    var onAttach = function(response, wcc) {
      webConsoleClient = wcc;

      if (response.error != null) {
        deferred.reject(response);
      }
      else {
        deferred.resolve(response);
      }

      // TODO: add _onTabNavigated code?
    };

    var listeners = [ 'PageError', 'ConsoleAPI' ];
    client.attachConsole(consoleActor, listeners, onAttach);

    return deferred.promise;
  });
};

/**
 * Execute some JavaScript
 */
exports.script.eval = function(javascript) {
  var deferred = promise.defer();

  var onResult = function(response) {
    var output = response.result;
    if (typeof output === 'object' && output.type === 'undefined') {
      output = undefined;
    }

    deferred.resolve({
      input: response.input,
      output: output,
      exception: response.exception
    });
  };

  webConsoleClient.evaluateJS(javascript, onResult, {});
  return deferred.promise;
};
