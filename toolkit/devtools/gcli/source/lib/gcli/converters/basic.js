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

var util = require('../util/util');

/**
 * Several converters are just data.toString inside a 'p' element
 */
function nodeFromDataToString(data, conversionContext) {
  var node = util.createElement(conversionContext.document, 'p');
  node.textContent = data.toString();
  return node;
}

exports.items = [
  {
    item: 'converter',
    from: 'string',
    to: 'dom',
    exec: nodeFromDataToString
  },
  {
    item: 'converter',
    from: 'number',
    to: 'dom',
    exec: nodeFromDataToString
  },
  {
    item: 'converter',
    from: 'boolean',
    to: 'dom',
    exec: nodeFromDataToString
  },
  {
    item: 'converter',
    from: 'undefined',
    to: 'dom',
    exec: function(data, conversionContext) {
      return util.createElement(conversionContext.document, 'span');
    }
  },
  {
    item: 'converter',
    from: 'number',
    to: 'string',
    exec: function(data) { return '' + data; }
  },
  {
    item: 'converter',
    from: 'boolean',
    to: 'string',
    exec: function(data) { return '' + data; }
  },
  {
    item: 'converter',
    from: 'undefined',
    to: 'string',
    exec: function(data) { return ''; }
  }
];
