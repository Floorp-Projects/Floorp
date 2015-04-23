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

var host = require('../util/host');
var prism = require('../util/prism').Prism;

function isMultiline(text) {
  return typeof text === 'string' && text.indexOf('\n') > -1;
}

exports.items = [
  {
    // Language implementation for Javascript
    item: 'language',
    name: 'javascript',
    prompt: '>',

    constructor: function(terminal) {
      this.document = terminal.document;
      this.focusManager = terminal.focusManager;

      this.updateHints();
    },

    destroy: function() {
      this.document = undefined;
    },

    exec: function(input) {
      return this.evaluate(input).then(function(response) {
        var output = (response.exception != null) ?
                      response.exception.class :
                      response.output;

        var isSameString = typeof output === 'string' &&
                           input.substr(1, input.length - 2) === output;
        var isSameOther = typeof output !== 'string' &&
                          input === '' + output;

        // Place strings in quotes
        if (typeof output === 'string' && response.exception == null) {
          if (output.indexOf('\'') === -1) {
            output = '\'' + output + '\'';
          }
          else {
            output = output.replace(/\\/, '\\').replace(/"/, '"').replace(/'/, '\'');
            output = '"' + output + '"';
          }
        }

        var line;
        if (isSameString || isSameOther || output === undefined) {
          line = input;
        }
        else if (isMultiline(output)) {
          line = input + '\n/*\n' + output + '\n*/';
        }
        else {
          line = input + ' // ' + output;
        }

        var grammar = prism.languages[this.name];
        return prism.highlight(line, grammar, this.name);
      }.bind(this));
    },

    evaluate: function(input) {
      return host.script.evaluate(input);
    }
  }
];
