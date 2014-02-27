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

var centralCanon = require('./commands/commands').centralCanon;
var connectors = require('./connectors/connectors');
var converters = require('./converters/converters');
var fields = require('./fields/fields');
var languages = require('./languages/languages');
var settings = require('./settings');
var centralTypes = require('./types/types').centralTypes;

/**
 * This is the heart of the API that we expose to the outside
 */
exports.getApi = function() {
  var canon = centralCanon;
  var types = centralTypes;

  settings.startup(types);

  var api = {
    addCommand: function(item) { return canon.addCommand(item); },
    removeCommand: function(item) { return canon.removeCommand(item); },
    addConnector: connectors.addConnector,
    removeConnector: connectors.removeConnector,
    addConverter: converters.addConverter,
    removeConverter: converters.removeConverter,
    addLanguage: languages.addLanguage,
    removeLanguage: languages.removeLanguage,
    addType: function(item) { return types.addType(item); },
    removeType: function(item) { return types.removeType(item); },

    addItems: function(items) {
      items.forEach(function(item) {
        // Some items are registered using the constructor so we need to check
        // the prototype for the the type of the item
        var type = item.item;
        if (type == null && item.prototype) {
          type = item.prototype.item;
        }
        if (type === 'command') {
          canon.addCommand(item);
        }
        else if (type === 'connector') {
          connectors.addConnector(item);
        }
        else if (type === 'converter') {
          converters.addConverter(item);
        }
        else if (type === 'field') {
          fields.addField(item);
        }
        else if (type === 'language') {
          languages.addLanguage(item);
        }
        else if (type === 'setting') {
          settings.addSetting(item);
        }
        else if (type === 'type') {
          types.addType(item);
        }
        else {
          console.error('Error for: ', item);
          throw new Error('item property not found');
        }
      });
    },

    removeItems: function(items) {
      items.forEach(function(item) {
        if (item.item === 'command') {
          canon.removeCommand(item);
        }
        else if (item.item === 'connector') {
          connectors.removeConnector(item);
        }
        else if (item.item === 'converter') {
          converters.removeConverter(item);
        }
        else if (item.item === 'field') {
          fields.removeField(item);
        }
        else if (item.item === 'language') {
          languages.removeLanguage(item);
        }
        else if (item.item === 'settings') {
          settings.removeSetting(types, item);
        }
        else if (item.item === 'type') {
          types.removeType(item);
        }
        else {
          throw new Error('item property not found');
        }
      });
    }
  };

  Object.defineProperty(api, 'canon', {
    get: function() { return canon; },
    set: function(c) { canon = c; },
    enumerable: true
  });

  Object.defineProperty(api, 'types', {
    get: function() { return types; },
    set: function(t) {
      types = t;
      settings.startup(types);
    },
    enumerable: true
  });

  return api;
};

/**
 * api.getApi() is clean, but generally we want to add the functions to the
 * 'exports' object. So this is a quick helper.
 */
exports.populateApi = function(obj) {
  var exportable = exports.getApi();
  Object.keys(exportable).forEach(function(key) {
    obj[key] = exportable[key];
  });
};
