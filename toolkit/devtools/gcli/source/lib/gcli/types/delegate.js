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
var Conversion = require('./types').Conversion;
var Status = require('./types').Status;

/**
 * The types we expose for registration
 */
exports.items = [
  // A type for "we don't know right now, but hope to soon"
  {
    item: 'type',
    name: 'delegate',

    constructor: function() {
      if (typeof this.delegateType !== 'function' &&
          typeof this.delegateType !== 'string') {
        throw new Error('Instances of DelegateType need typeSpec.delegateType' +
                        ' to be a function that returns a type');
      }
    },

    getSpec: function(commandName, paramName) {
      return {
        name: 'delegate',
        param: paramName
      };
    },

    // Child types should implement this method to return an instance of the type
    // that should be used. If no type is available, or some sort of temporary
    // placeholder is required, BlankType can be used.
    delegateType: function(context) {
      throw new Error('Not implemented');
    },

    stringify: function(value, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.stringify(value, context);
      }.bind(this));
    },

    parse: function(arg, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.parse(arg, context);
      }.bind(this));
    },

    decrement: function(value, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.decrement ?
               delegated.decrement(value, context) :
               undefined;
      }.bind(this));
    },

    increment: function(value, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.increment ?
               delegated.increment(value, context) :
               undefined;
      }.bind(this));
    },

    getType: function(context) {
      var type = this.delegateType(context);
      if (typeof type.parse !== 'function') {
        type = this.types.createType(type);
      }
      return Promise.resolve(type);
    },

    // DelegateType is designed to be inherited from, so DelegateField needs a way
    // to check if something works like a delegate without using 'name'
    isDelegate: true,

    // Technically we perhaps should proxy this, except that properties are
    // inherently synchronous, so we can't. It doesn't seem important enough to
    // change the function definition to accommodate this right now
    isImportant: false
  },
  {
    item: 'type',
    name: 'remote',
    param: undefined,

    stringify: function(value, context) {
      // remote types are client only, and we don't attempt to transfer value
      // objects to the client (we can't be sure the are jsonable) so it is a
      // but strange to be asked to stringify a value object, however since
      // parse creates a Conversion with a (fake) value object we might be
      // asked to stringify that. We can stringify fake value objects.
      if (typeof value.stringified === 'string') {
        return value.stringified;
      }
      throw new Error('Can\'t stringify that value');
    },

    parse: function(arg, context) {
      var args = { typed: context.typed, param: this.param };
      return this.connection.call('typeparse', args).then(function(json) {
        var status = Status.fromString(json.status);
        var val = { stringified: arg.text };
        return new Conversion(val, arg, status, json.message, json.predictions);
      });
    },

    decrement: function(value, context) {
      var args = { typed: context.typed, param: this.param };
      return this.connection.call('typedecrement', args).then(function(json) {
        return { stringified: json.arg };
      });
    },

    increment: function(value, context) {
      var args = { typed: context.typed, param: this.param };
      return this.connection.call('typeincrement', args).then(function(json) {
        return { stringified: json.arg };
      });
    }
  },
  // 'blank' is a type for use with DelegateType when we don't know yet.
  // It should not be used anywhere else.
  {
    item: 'type',
    name: 'blank',

    getSpec: function(commandName, paramName) {
      return 'blank';
    },

    stringify: function(value, context) {
      return '';
    },

    parse: function(arg, context) {
      return Promise.resolve(new Conversion(undefined, arg));
    }
  }
];
