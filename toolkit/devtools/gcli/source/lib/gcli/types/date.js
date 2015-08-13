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

var l10n = require('../util/l10n');
var Status = require('./types').Status;
var Conversion = require('./types').Conversion;

/**
 * Helper for stringify() to left pad a single digit number with a single '0'
 * so 1 -> '01', 42 -> '42', etc.
 */
function pad(number) {
  var r = String(number);
  return r.length === 1 ? '0' + r : r;
}

/**
 * Utility to convert a string to a date, throwing if the date can't be
 * parsed rather than having an invalid date
 */
function toDate(str) {
  var millis = Date.parse(str);
  if (isNaN(millis)) {
    throw new Error(l10n.lookupFormat('typesDateNan', [ str ]));
  }
  return new Date(millis);
}

/**
 * Is |thing| a valid date?
 * @see http://stackoverflow.com/questions/1353684/detecting-an-invalid-date-date-instance-in-javascript
 */
function isDate(thing) {
  return Object.prototype.toString.call(thing) === '[object Date]'
          && !isNaN(thing.getTime());
}

exports.items = [
  {
    // ECMA 5.1 §15.9.1.1
    // @see http://stackoverflow.com/questions/11526504/minimum-and-maximum-date
    item: 'type',
    name: 'date',
    step: 1,
    min: new Date(-8640000000000000),
    max: new Date(8640000000000000),

    constructor: function() {
      this._origMin = this.min;
      if (this.min != null) {
        if (typeof this.min === 'string') {
          this.min = toDate(this.min);
        }
        else if (isDate(this.min) || typeof this.min === 'function') {
          this.min = this.min;
        }
        else {
          throw new Error('date min value must be one of string/date/function');
        }
      }

      this._origMax = this.max;
      if (this.max != null) {
        if (typeof this.max === 'string') {
          this.max = toDate(this.max);
        }
        else if (isDate(this.max) || typeof this.max === 'function') {
          this.max = this.max;
        }
        else {
          throw new Error('date max value must be one of string/date/function');
        }
      }
    },

    getSpec: function() {
      var spec = {
        name: 'date'
      };
      if (this.step !== 1) {
        spec.step = this.step;
      }
      if (this._origMax != null) {
        spec.max = this._origMax;
      }
      if (this._origMin != null) {
        spec.min = this._origMin;
      }
      return spec;
    },

    stringify: function(value, context) {
      if (!isDate(value)) {
        return '';
      }

      var str = pad(value.getFullYear()) + '-' +
                pad(value.getMonth() + 1) + '-' +
                pad(value.getDate());

      // Only add in the time if it's not midnight
      if (value.getHours() !== 0 || value.getMinutes() !== 0 ||
          value.getSeconds() !== 0 || value.getMilliseconds() !== 0) {

        // What string should we use to separate the date from the time?
        // There are 3 options:
        // 'T': This is the standard from ISO8601. i.e. 2013-05-20T11:05
        //      The good news - it's a standard. The bad news - it's weird and
        //      alien to many if not most users
        // ' ': This looks nicest, but needs escaping (which GCLI will do
        //      automatically) so it would look like: '2013-05-20 11:05'
        //      Good news: looks best, bad news: on completion we place the
        //      cursor after the final ', breaking repeated increment/decrement
        // '\ ': It's possible that we could find a way to use a \ to escape
        //      the space, so the output would look like: 2013-05-20\ 11:05
        //      This would involve changes to a number of parts, and is
        //      probably too complex a solution for this problem for now
        // In the short term I'm going for ' ', and raising the priority of
        // cursor positioning on actions like increment/decrement/tab.

        str += ' ' + pad(value.getHours());
        str += ':' + pad(value.getMinutes());

        // Only add in seconds/milliseconds if there is anything to report
        if (value.getSeconds() !== 0 || value.getMilliseconds() !== 0) {
          str += ':' + pad(value.getSeconds());
          if (value.getMilliseconds() !== 0) {
            var milliVal = (value.getUTCMilliseconds() / 1000).toFixed(3);
            str += '.' + String(milliVal).slice(2, 5);
          }
        }
      }

      return str;
    },

    getMax: function(context) {
      if (typeof this.max === 'function') {
        return this._max(context);
      }
      if (isDate(this.max)) {
        return this.max;
      }
      return undefined;
    },

    getMin: function(context) {
      if (typeof this.min === 'function') {
        return this._min(context);
      }
      if (isDate(this.min)) {
        return this.min;
      }
      return undefined;
    },

    parse: function(arg, context) {
      var value;

      if (arg.text.replace(/\s/g, '').length === 0) {
        return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE, ''));
      }

      // Lots of room for improvement here: 1h ago, in two days, etc.
      // Should "1h ago" dynamically update the step?
      if (arg.text.toLowerCase() === 'now' ||
          arg.text.toLowerCase() === 'today') {
        value = new Date();
      }
      else if (arg.text.toLowerCase() === 'yesterday') {
        value = new Date();
        value.setDate(value.getDate() - 1);
      }
      else if (arg.text.toLowerCase() === 'tomorrow') {
        value = new Date();
        value.setDate(value.getDate() + 1);
      }
      else {
        // So now actual date parsing.
        // Javascript dates are a mess. Like the default date libraries in most
        // common languages, but with added browser weirdness.
        // There is an argument for saying that the user will expect dates to
        // be formatted as JavaScript dates, except that JS dates are of
        // themselves very unexpected.
        // See http://blog.dygraphs.com/2012/03/javascript-and-dates-what-mess.html

        // The timezone used by Date.parse depends on whether or not the string
        // can be interpreted as ISO-8601, so "2000-01-01" is not the same as
        // "2000/01/01" (unless your TZ aligns with UTC) because the first is
        // ISO-8601 and therefore assumed to be UTC, where the latter is
        // assumed to be in the local timezone.

        // First, if the user explicitly includes a 'Z' timezone marker, then
        // we assume they know what they are doing with timezones. ISO-8601
        // uses 'Z' as a marker for 'Zulu time', zero hours offset i.e. UTC
        if (arg.text.indexOf('Z') !== -1) {
          value = new Date(arg.text);
        }
        else {
          // Now we don't want the browser to assume ISO-8601 and therefore use
          // UTC so we replace the '-' with '/'
          value = new Date(arg.text.replace(/-/g, '/'));
        }

        if (isNaN(value.getTime())) {
          var msg = l10n.lookupFormat('typesDateNan', [ arg.text ]);
          return Promise.resolve(new Conversion(undefined, arg, Status.ERROR, msg));
        }
      }

      return Promise.resolve(new Conversion(value, arg));
    },

    nudge: function(value, by, context) {
      if (!isDate(value)) {
        return new Date();
      }

      var newValue = new Date(value);
      newValue.setDate(value.getDate() + (by * this.step));

      if (newValue < this.getMin(context)) {
        return this.getMin(context);
      }
      else if (newValue > this.getMax(context)) {
        return this.getMax();
      }
      else {
        return newValue;
      }
    }
  }
];
