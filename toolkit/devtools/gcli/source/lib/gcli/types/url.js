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
var Promise = require('../util/promise').Promise;
var Status = require('./types').Status;
var Conversion = require('./types').Conversion;

exports.items = [
  {
    item: 'type',
    name: 'url',

    getSpec: function() {
      return 'url';
    },

    stringify: function(value, context) {
      if (value == null) {
        return '';
      }
      return value.href;
    },

    parse: function(arg, context) {
      var conversion;

      try {
        var url = host.createUrl(arg.text);
        conversion = new Conversion(url, arg);
      }
      catch (ex) {
        var predictions = [];
        var status = Status.ERROR;

        // Maybe the URL was missing a scheme?
        if (arg.text.indexOf('://') === -1) {
          [ 'http', 'https' ].forEach(function(scheme) {
            try {
              var http = host.createUrl(scheme + '://' + arg.text);
              predictions.push({ name: http.href, value: http });
            }
            catch (ex) {
              // Ignore
            }
          }.bind(this));

          // Try to create a URL with the current page as a base ref
          if (context.environment.window) {
            try {
              var base = context.environment.window.location.href;
              var localized = host.createUrl(arg.text, base);
              predictions.push({ name: localized.href, value: localized });
            }
            catch (ex) {
              // Ignore
            }
          }
        }

        if (predictions.length > 0) {
          status = Status.INCOMPLETE;
        }

        conversion = new Conversion(undefined, arg, status,
                                    ex.message, predictions);
      }

      return Promise.resolve(conversion);
    }
  }
];
