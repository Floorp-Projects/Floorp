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

// Warning - gcli.js causes this version of host.js to be favored in NodeJS
// which means that it's also used in testing in NodeJS

var childProcess = require('child_process');
var fs = require('fs');
var path = require('path');

var promise = require('./promise');
var util = require('./util');

var ATTR_NAME = '__gcli_border';
var HIGHLIGHT_STYLE = '1px dashed black';

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
  if (node.hasAttribute(ATTR_NAME)) {
    return;
  }

  var styles = this._document.defaultView.getComputedStyle(node);
  node.setAttribute(ATTR_NAME, styles.border);
  node.style.border = HIGHLIGHT_STYLE;
};

Highlighter.prototype._unhighlightNode = function(node) {
  var previous = node.getAttribute(ATTR_NAME);
  node.style.border = previous;
  node.removeAttribute(ATTR_NAME);
};

exports.Highlighter = Highlighter;

/**
 * See docs in lib/gcli/util/host.js:exec
 */
exports.exec = function(execSpec) {
  var deferred = promise.defer();

  var output = { data: '' };
  var child = childProcess.spawn(execSpec.cmd, execSpec.args, {
    env: execSpec.env,
    cwd: execSpec.cwd
  });

  child.stdout.on('data', function(data) {
    output.data += data;
  });

  child.stderr.on('data', function(data) {
    output.data += data;
  });

  child.on('close', function(code) {
    output.code = code;
    if (code === 0) {
      deferred.resolve(output);
    }
    else {
      deferred.reject(output);
    }
  });

  return deferred.promise;
};

/**
 * Asynchronously load a text resource
 * @param requistingModule Typically just 'module' to pick up the 'module'
 * variable from the calling modules scope
 * @param name The name of the resource to load, as a path (including extension)
 * relative to that of the requiring module
 * @return A promise of the contents of the file as a string
 */
exports.staticRequire = function(requistingModule, name) {
  var deferred = promise.defer();

  var parent = path.dirname(requistingModule.id);
  var filename = parent + '/' + name;

  fs.readFile(filename, { encoding: 'utf8' }, function(err, data) {
    if (err) {
      deferred.reject(err);
    }

    deferred.resolve(data);
  });

  return deferred.promise;
};

/**
 * A group of functions to help scripting. Small enough that it doesn't need
 * a separate module (it's basically a wrapper around 'eval' in some contexts)
 */
exports.script = {
  onOutput: util.createEvent('Script.onOutput'),

  // Setup the environment to eval JavaScript, a no-op on the web
  useTarget: function(tgt) { },

  // Execute some JavaScript
  eval: function(javascript) {
    try {
      return promise.resolve({
        input: javascript,
        output: eval(javascript),
        exception: null
      });
    }
    catch (ex) {
      return promise.resolve({
        input: javascript,
        output: null,
        exception: ex
      });
    }
  }
};
