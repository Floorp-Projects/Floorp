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

/**
 * This is a quick and dirty stub that allows us to write code in remoted.js
 * that looks like gcli.js
 */
exports.method = function(func, spec) {
  // An array of strings, being the names of the parameters
  var argSpecs = [];
  if (spec.request != null) {
    Object.keys(spec.request).forEach(function(name) {
      var arg = spec.request[name];
      argSpecs[arg.index] = name;
    });
  }

  return function(data) {
    var args = (data == null) ?
               [] :
               argSpecs.map(function(name) { return data[name]; });
    return func.apply(this, args);
  };
};

var Arg = exports.Arg = function(index, type) {
  if (this == null) {
    return new Arg(index, type);
  }

  this.index = index;
  this.type = type;
};

var RetVal = exports.RetVal = function(type) {
  if (this == null) {
    return new RetVal(type);
  }

  this.type = type;
};
