(function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
"use strict";

var add = require("./lib/add");
var subtract = require("./lib/subtract");
var upperCase = require("upper-case");

(function () {
  return add(1, 2);
});

},{"./lib/add":2,"./lib/subtract":3,"upper-case":4}],2:[function(require,module,exports){
"use strict";

module.exports = function (a, b) {
  return a + b;
};

},{}],3:[function(require,module,exports){
"use strict";

module.exports = function (a, b) {
  return a - b;
};

},{}],4:[function(require,module,exports){
/**
 * Special language-specific overrides.
 *
 * Source: ftp://ftp.unicode.org/Public/UCD/latest/ucd/SpecialCasing.txt
 *
 * @type {Object}
 */
var LANGUAGES = {
  tr: {
    regexp: /[\u0069]/g,
    map: {
      '\u0069': '\u0130'
    }
  },
  az: {
    regexp: /[\u0069]/g,
    map: {
      '\u0069': '\u0130'
    }
  },
  lt: {
    regexp: /[\u0069\u006A\u012F]\u0307|\u0069\u0307[\u0300\u0301\u0303]/g,
    map: {
      '\u0069\u0307': '\u0049',
      '\u006A\u0307': '\u004A',
      '\u012F\u0307': '\u012E',
      '\u0069\u0307\u0300': '\u00CC',
      '\u0069\u0307\u0301': '\u00CD',
      '\u0069\u0307\u0303': '\u0128'
    }
  }
}

/**
 * Upper case a string.
 *
 * @param  {String} str
 * @return {String}
 */
module.exports = function (str, locale) {
  var lang = LANGUAGES[locale]

  str = str == null ? '' : String(str)

  if (lang) {
    str = str.replace(lang.regexp, function (m) { return lang.map[m] })
  }

  return str.toUpperCase()
}

},{}]},{},[1])
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIi4uLy4uLy4uLy4uL3Vzci9sb2NhbC9saWIvbm9kZV9tb2R1bGVzL2Jyb3dzZXJpZnkvbm9kZV9tb2R1bGVzL2Jyb3dzZXItcGFjay9fcHJlbHVkZS5qcyIsIi9Vc2Vycy9qc2FudGVsbC9EZXYvc291cmNlLW1hcC10ZXN0L2luZGV4LmpzIiwiL1VzZXJzL2pzYW50ZWxsL0Rldi9zb3VyY2UtbWFwLXRlc3QvbGliL2FkZC5qcyIsIi9Vc2Vycy9qc2FudGVsbC9EZXYvc291cmNlLW1hcC10ZXN0L2xpYi9zdWJ0cmFjdC5qcyIsIm5vZGVfbW9kdWxlcy91cHBlci1jYXNlL3VwcGVyLWNhc2UuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUE7OztBQ0FBLElBQUksR0FBRyxHQUFHLE9BQU8sQ0FBQyxXQUFXLENBQUMsQ0FBQztBQUMvQixJQUFJLFFBQVEsR0FBRyxPQUFPLENBQUMsZ0JBQWdCLENBQUMsQ0FBQztBQUN6QyxJQUFJLFNBQVMsR0FBRyxPQUFPLENBQUMsWUFBWSxDQUFDLENBQUM7O0FBRXRDLENBQUM7U0FBTSxHQUFHLENBQUMsQ0FBQyxFQUFDLENBQUMsQ0FBQztFQUFBLENBQUU7Ozs7O0FDSmpCLE1BQU0sQ0FBQyxPQUFPLEdBQUcsVUFBVSxDQUFDLEVBQUUsQ0FBQyxFQUFFO0FBQy9CLFNBQU8sQ0FBQyxHQUFHLENBQUMsQ0FBQztDQUNkLENBQUM7Ozs7O0FDRkYsTUFBTSxDQUFDLE9BQU8sR0FBRyxVQUFVLENBQUMsRUFBRSxDQUFDLEVBQUU7QUFDL0IsU0FBTyxDQUFDLEdBQUcsQ0FBQyxDQUFDO0NBQ2QsQ0FBQzs7O0FDRkY7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBIiwiZmlsZSI6ImdlbmVyYXRlZC5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzQ29udGVudCI6WyIoZnVuY3Rpb24gZSh0LG4scil7ZnVuY3Rpb24gcyhvLHUpe2lmKCFuW29dKXtpZighdFtvXSl7dmFyIGE9dHlwZW9mIHJlcXVpcmU9PVwiZnVuY3Rpb25cIiYmcmVxdWlyZTtpZighdSYmYSlyZXR1cm4gYShvLCEwKTtpZihpKXJldHVybiBpKG8sITApO3ZhciBmPW5ldyBFcnJvcihcIkNhbm5vdCBmaW5kIG1vZHVsZSAnXCIrbytcIidcIik7dGhyb3cgZi5jb2RlPVwiTU9EVUxFX05PVF9GT1VORFwiLGZ9dmFyIGw9bltvXT17ZXhwb3J0czp7fX07dFtvXVswXS5jYWxsKGwuZXhwb3J0cyxmdW5jdGlvbihlKXt2YXIgbj10W29dWzFdW2VdO3JldHVybiBzKG4/bjplKX0sbCxsLmV4cG9ydHMsZSx0LG4scil9cmV0dXJuIG5bb10uZXhwb3J0c312YXIgaT10eXBlb2YgcmVxdWlyZT09XCJmdW5jdGlvblwiJiZyZXF1aXJlO2Zvcih2YXIgbz0wO288ci5sZW5ndGg7bysrKXMocltvXSk7cmV0dXJuIHN9KSIsInZhciBhZGQgPSByZXF1aXJlKFwiLi9saWIvYWRkXCIpO1xudmFyIHN1YnRyYWN0ID0gcmVxdWlyZShcIi4vbGliL3N1YnRyYWN0XCIpO1xudmFyIHVwcGVyQ2FzZSA9IHJlcXVpcmUoXCJ1cHBlci1jYXNlXCIpO1xuXG4oKCkgPT4gYWRkKDEsMikpO1xuIiwibW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoYSwgYikge1xuICByZXR1cm4gYSArIGI7XG59O1xuIiwibW9kdWxlLmV4cG9ydHMgPSBmdW5jdGlvbiAoYSwgYikge1xuICByZXR1cm4gYSAtIGI7XG59O1xuIiwiLyoqXG4gKiBTcGVjaWFsIGxhbmd1YWdlLXNwZWNpZmljIG92ZXJyaWRlcy5cbiAqXG4gKiBTb3VyY2U6IGZ0cDovL2Z0cC51bmljb2RlLm9yZy9QdWJsaWMvVUNEL2xhdGVzdC91Y2QvU3BlY2lhbENhc2luZy50eHRcbiAqXG4gKiBAdHlwZSB7T2JqZWN0fVxuICovXG52YXIgTEFOR1VBR0VTID0ge1xuICB0cjoge1xuICAgIHJlZ2V4cDogL1tcXHUwMDY5XS9nLFxuICAgIG1hcDoge1xuICAgICAgJ1xcdTAwNjknOiAnXFx1MDEzMCdcbiAgICB9XG4gIH0sXG4gIGF6OiB7XG4gICAgcmVnZXhwOiAvW1xcdTAwNjldL2csXG4gICAgbWFwOiB7XG4gICAgICAnXFx1MDA2OSc6ICdcXHUwMTMwJ1xuICAgIH1cbiAgfSxcbiAgbHQ6IHtcbiAgICByZWdleHA6IC9bXFx1MDA2OVxcdTAwNkFcXHUwMTJGXVxcdTAzMDd8XFx1MDA2OVxcdTAzMDdbXFx1MDMwMFxcdTAzMDFcXHUwMzAzXS9nLFxuICAgIG1hcDoge1xuICAgICAgJ1xcdTAwNjlcXHUwMzA3JzogJ1xcdTAwNDknLFxuICAgICAgJ1xcdTAwNkFcXHUwMzA3JzogJ1xcdTAwNEEnLFxuICAgICAgJ1xcdTAxMkZcXHUwMzA3JzogJ1xcdTAxMkUnLFxuICAgICAgJ1xcdTAwNjlcXHUwMzA3XFx1MDMwMCc6ICdcXHUwMENDJyxcbiAgICAgICdcXHUwMDY5XFx1MDMwN1xcdTAzMDEnOiAnXFx1MDBDRCcsXG4gICAgICAnXFx1MDA2OVxcdTAzMDdcXHUwMzAzJzogJ1xcdTAxMjgnXG4gICAgfVxuICB9XG59XG5cbi8qKlxuICogVXBwZXIgY2FzZSBhIHN0cmluZy5cbiAqXG4gKiBAcGFyYW0gIHtTdHJpbmd9IHN0clxuICogQHJldHVybiB7U3RyaW5nfVxuICovXG5tb2R1bGUuZXhwb3J0cyA9IGZ1bmN0aW9uIChzdHIsIGxvY2FsZSkge1xuICB2YXIgbGFuZyA9IExBTkdVQUdFU1tsb2NhbGVdXG5cbiAgc3RyID0gc3RyID09IG51bGwgPyAnJyA6IFN0cmluZyhzdHIpXG5cbiAgaWYgKGxhbmcpIHtcbiAgICBzdHIgPSBzdHIucmVwbGFjZShsYW5nLnJlZ2V4cCwgZnVuY3Rpb24gKG0pIHsgcmV0dXJuIGxhbmcubWFwW21dIH0pXG4gIH1cblxuICByZXR1cm4gc3RyLnRvVXBwZXJDYXNlKClcbn1cbiJdfQ==
