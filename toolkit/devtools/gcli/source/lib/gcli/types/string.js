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

var promise = require('../util/promise');
var Status = require('./types').Status;
var Conversion = require('./types').Conversion;

exports.items = [
  {
    // 'string' the most basic string type where all we need to do is to take
    // care of converting escaped characters like \t, \n, etc.
    // For the full list see
    // https://developer.mozilla.org/en-US/docs/JavaScript/Guide/Values,_variables,_and_literals
    // The exception is that we ignore \b because replacing '\b' characters in
    // stringify() with their escaped version injects '\\b' all over the place
    // and the need to support \b seems low)
    // Customizations:
    // allowBlank: Allow a blank string to be counted as valid
    item: 'type',
    name: 'string',
    allowBlank: false,

    getSpec: function() {
      return this.allowBlank ?
             { name: 'string', allowBlank: true } :
             'string';
    },

    stringify: function(value, context) {
      if (value == null) {
        return '';
      }

      return value
           .replace(/\\/g, '\\\\')
           .replace(/\f/g, '\\f')
           .replace(/\n/g, '\\n')
           .replace(/\r/g, '\\r')
           .replace(/\t/g, '\\t')
           .replace(/\v/g, '\\v')
           .replace(/\n/g, '\\n')
           .replace(/\r/g, '\\r')
           .replace(/ /g, '\\ ')
           .replace(/'/g, '\\\'')
           .replace(/"/g, '\\"')
           .replace(/{/g, '\\{')
           .replace(/}/g, '\\}');
    },

    parse: function(arg, context) {
      if (!this.allowBlank && (arg.text == null || arg.text === '')) {
        return promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE, ''));
      }

      // The string '\\' (i.e. an escaped \ (represented here as '\\\\' because it
      // is double escaped)) is first converted to a private unicode character and
      // then at the end from \uF000 to a single '\' to avoid the string \\n being
      // converted first to \n and then to a <LF>
      var value = arg.text
           .replace(/\\\\/g, '\uF000')
           .replace(/\\f/g, '\f')
           .replace(/\\n/g, '\n')
           .replace(/\\r/g, '\r')
           .replace(/\\t/g, '\t')
           .replace(/\\v/g, '\v')
           .replace(/\\n/g, '\n')
           .replace(/\\r/g, '\r')
           .replace(/\\ /g, ' ')
           .replace(/\\'/g, '\'')
           .replace(/\\"/g, '"')
           .replace(/\\{/g, '{')
           .replace(/\\}/g, '}')
           .replace(/\uF000/g, '\\');

      return promise.resolve(new Conversion(value, arg));
    }
  }
];
