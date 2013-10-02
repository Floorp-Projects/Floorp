let window = self;
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

/**
 * Define a module along with a payload.
 * @param {string} moduleName Name for the payload
 * @param {ignored} deps Ignored. For compatibility with CommonJS AMD Spec
 * @param {function} payload Function with (require, exports, module) params
 */
function define(moduleName, deps, payload) {
  if (typeof moduleName != "string") {
    throw new TypeError('Expected string, got: ' + moduleName);
  }

  if (arguments.length == 2) {
    payload = deps;
  }

  if (moduleName in define.modules) {
    throw new Error("Module already defined: " + moduleName);
  }
  define.modules[moduleName] = payload;
};

/**
 * The global store of un-instantiated modules
 */
define.modules = {};


/**
 * We invoke require() in the context of a Domain so we can have multiple
 * sets of modules running separate from each other.
 * This contrasts with JSMs which are singletons, Domains allows us to
 * optionally load a CommonJS module twice with separate data each time.
 * Perhaps you want 2 command lines with a different set of commands in each,
 * for example.
 */
function Domain() {
  this.modules = {};
  this._currentModule = null;
}

(function () {

  /**
   * Lookup module names and resolve them by calling the definition function if
   * needed.
   * There are 2 ways to call this, either with an array of dependencies and a
   * callback to call when the dependencies are found (which can happen
   * asynchronously in an in-page context) or with a single string an no callback
   * where the dependency is resolved synchronously and returned.
   * The API is designed to be compatible with the CommonJS AMD spec and
   * RequireJS.
   * @param {string[]|string} deps A name, or names for the payload
   * @param {function|undefined} callback Function to call when the dependencies
   * are resolved
   * @return {undefined|object} The module required or undefined for
   * array/callback method
   */
  Domain.prototype.require = function(deps, callback) {
    if (Array.isArray(deps)) {
      var params = deps.map(function(dep) {
        return this.lookup(dep);
      }, this);
      if (callback) {
        callback.apply(null, params);
      }
      return undefined;
    }
    else {
      return this.lookup(deps);
    }
  };

  function normalize(path) {
    var bits = path.split('/');
    var i = 1;
    while (i < bits.length) {
      if (bits[i] === '..') {
        bits.splice(i-1, 1);
      } else if (bits[i] === '.') {
        bits.splice(i, 1);
      } else {
        i++;
      }
    }
    return bits.join('/');
  }

  function join(a, b) {
    a = a.trim();
    b = b.trim();
    if (/^\//.test(b)) {
      return b;
    } else {
      return a.replace(/\/*$/, '/') + b;
    }
  }

  function dirname(path) {
    var bits = path.split('/');
    bits.pop();
    return bits.join('/');
  }

  /**
   * Lookup module names and resolve them by calling the definition function if
   * needed.
   * @param {string} moduleName A name for the payload to lookup
   * @return {object} The module specified by aModuleName or null if not found.
   */
  Domain.prototype.lookup = function(moduleName) {
    if (/^\./.test(moduleName)) {
      moduleName = normalize(join(dirname(this._currentModule), moduleName));
    }

    if (moduleName in this.modules) {
      var module = this.modules[moduleName];
      return module;
    }

    if (!(moduleName in define.modules)) {
      throw new Error("Module not defined: " + moduleName);
    }

    var module = define.modules[moduleName];

    if (typeof module == "function") {
      var exports = {};
      var previousModule = this._currentModule;
      this._currentModule = moduleName;
      module(this.require.bind(this), exports, { id: moduleName, uri: "" });
      this._currentModule = previousModule;
      module = exports;
    }

    // cache the resulting module object for next time
    this.modules[moduleName] = module;

    return module;
  };

}());

define.Domain = Domain;
define.globalDomain = new Domain();
var require = define.globalDomain.require.bind(define.globalDomain);
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/source-map-generator', ['require', 'exports', 'module' ,  'source-map/base64-vlq', 'source-map/util', 'source-map/array-set'], function(require, exports, module) {

  var base64VLQ = require('./base64-vlq');
  var util = require('./util');
  var ArraySet = require('./array-set').ArraySet;

  /**
   * An instance of the SourceMapGenerator represents a source map which is
   * being built incrementally. To create a new one, you must pass an object
   * with the following properties:
   *
   *   - file: The filename of the generated source.
   *   - sourceRoot: An optional root for all URLs in this source map.
   */
  function SourceMapGenerator(aArgs) {
    this._file = util.getArg(aArgs, 'file');
    this._sourceRoot = util.getArg(aArgs, 'sourceRoot', null);
    this._sources = new ArraySet();
    this._names = new ArraySet();
    this._mappings = [];
    this._sourcesContents = null;
  }

  SourceMapGenerator.prototype._version = 3;

  /**
   * Creates a new SourceMapGenerator based on a SourceMapConsumer
   *
   * @param aSourceMapConsumer The SourceMap.
   */
  SourceMapGenerator.fromSourceMap =
    function SourceMapGenerator_fromSourceMap(aSourceMapConsumer) {
      var sourceRoot = aSourceMapConsumer.sourceRoot;
      var generator = new SourceMapGenerator({
        file: aSourceMapConsumer.file,
        sourceRoot: sourceRoot
      });
      aSourceMapConsumer.eachMapping(function (mapping) {
        var newMapping = {
          generated: {
            line: mapping.generatedLine,
            column: mapping.generatedColumn
          }
        };

        if (mapping.source) {
          newMapping.source = mapping.source;
          if (sourceRoot) {
            newMapping.source = util.relative(sourceRoot, newMapping.source);
          }

          newMapping.original = {
            line: mapping.originalLine,
            column: mapping.originalColumn
          };

          if (mapping.name) {
            newMapping.name = mapping.name;
          }
        }

        generator.addMapping(newMapping);
      });
      aSourceMapConsumer.sources.forEach(function (sourceFile) {
        var content = aSourceMapConsumer.sourceContentFor(sourceFile);
        if (content) {
          generator.setSourceContent(sourceFile, content);
        }
      });
      return generator;
    };

  /**
   * Add a single mapping from original source line and column to the generated
   * source's line and column for this source map being created. The mapping
   * object should have the following properties:
   *
   *   - generated: An object with the generated line and column positions.
   *   - original: An object with the original line and column positions.
   *   - source: The original source file (relative to the sourceRoot).
   *   - name: An optional original token name for this mapping.
   */
  SourceMapGenerator.prototype.addMapping =
    function SourceMapGenerator_addMapping(aArgs) {
      var generated = util.getArg(aArgs, 'generated');
      var original = util.getArg(aArgs, 'original', null);
      var source = util.getArg(aArgs, 'source', null);
      var name = util.getArg(aArgs, 'name', null);

      this._validateMapping(generated, original, source, name);

      if (source && !this._sources.has(source)) {
        this._sources.add(source);
      }

      if (name && !this._names.has(name)) {
        this._names.add(name);
      }

      this._mappings.push({
        generatedLine: generated.line,
        generatedColumn: generated.column,
        originalLine: original != null && original.line,
        originalColumn: original != null && original.column,
        source: source,
        name: name
      });
    };

  /**
   * Set the source content for a source file.
   */
  SourceMapGenerator.prototype.setSourceContent =
    function SourceMapGenerator_setSourceContent(aSourceFile, aSourceContent) {
      var source = aSourceFile;
      if (this._sourceRoot) {
        source = util.relative(this._sourceRoot, source);
      }

      if (aSourceContent !== null) {
        // Add the source content to the _sourcesContents map.
        // Create a new _sourcesContents map if the property is null.
        if (!this._sourcesContents) {
          this._sourcesContents = {};
        }
        this._sourcesContents[util.toSetString(source)] = aSourceContent;
      } else {
        // Remove the source file from the _sourcesContents map.
        // If the _sourcesContents map is empty, set the property to null.
        delete this._sourcesContents[util.toSetString(source)];
        if (Object.keys(this._sourcesContents).length === 0) {
          this._sourcesContents = null;
        }
      }
    };

  /**
   * Applies the mappings of a sub-source-map for a specific source file to the
   * source map being generated. Each mapping to the supplied source file is
   * rewritten using the supplied source map. Note: The resolution for the
   * resulting mappings is the minimium of this map and the supplied map.
   *
   * @param aSourceMapConsumer The source map to be applied.
   * @param aSourceFile Optional. The filename of the source file.
   *        If omitted, SourceMapConsumer's file property will be used.
   */
  SourceMapGenerator.prototype.applySourceMap =
    function SourceMapGenerator_applySourceMap(aSourceMapConsumer, aSourceFile) {
      // If aSourceFile is omitted, we will use the file property of the SourceMap
      if (!aSourceFile) {
        aSourceFile = aSourceMapConsumer.file;
      }
      var sourceRoot = this._sourceRoot;
      // Make "aSourceFile" relative if an absolute Url is passed.
      if (sourceRoot) {
        aSourceFile = util.relative(sourceRoot, aSourceFile);
      }
      // Applying the SourceMap can add and remove items from the sources and
      // the names array.
      var newSources = new ArraySet();
      var newNames = new ArraySet();

      // Find mappings for the "aSourceFile"
      this._mappings.forEach(function (mapping) {
        if (mapping.source === aSourceFile && mapping.originalLine) {
          // Check if it can be mapped by the source map, then update the mapping.
          var original = aSourceMapConsumer.originalPositionFor({
            line: mapping.originalLine,
            column: mapping.originalColumn
          });
          if (original.source !== null) {
            // Copy mapping
            if (sourceRoot) {
              mapping.source = util.relative(sourceRoot, original.source);
            } else {
              mapping.source = original.source;
            }
            mapping.originalLine = original.line;
            mapping.originalColumn = original.column;
            if (original.name !== null && mapping.name !== null) {
              // Only use the identifier name if it's an identifier
              // in both SourceMaps
              mapping.name = original.name;
            }
          }
        }

        var source = mapping.source;
        if (source && !newSources.has(source)) {
          newSources.add(source);
        }

        var name = mapping.name;
        if (name && !newNames.has(name)) {
          newNames.add(name);
        }

      }, this);
      this._sources = newSources;
      this._names = newNames;

      // Copy sourcesContents of applied map.
      aSourceMapConsumer.sources.forEach(function (sourceFile) {
        var content = aSourceMapConsumer.sourceContentFor(sourceFile);
        if (content) {
          if (sourceRoot) {
            sourceFile = util.relative(sourceRoot, sourceFile);
          }
          this.setSourceContent(sourceFile, content);
        }
      }, this);
    };

  /**
   * A mapping can have one of the three levels of data:
   *
   *   1. Just the generated position.
   *   2. The Generated position, original position, and original source.
   *   3. Generated and original position, original source, as well as a name
   *      token.
   *
   * To maintain consistency, we validate that any new mapping being added falls
   * in to one of these categories.
   */
  SourceMapGenerator.prototype._validateMapping =
    function SourceMapGenerator_validateMapping(aGenerated, aOriginal, aSource,
                                                aName) {
      if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
          && aGenerated.line > 0 && aGenerated.column >= 0
          && !aOriginal && !aSource && !aName) {
        // Case 1.
        return;
      }
      else if (aGenerated && 'line' in aGenerated && 'column' in aGenerated
               && aOriginal && 'line' in aOriginal && 'column' in aOriginal
               && aGenerated.line > 0 && aGenerated.column >= 0
               && aOriginal.line > 0 && aOriginal.column >= 0
               && aSource) {
        // Cases 2 and 3.
        return;
      }
      else {
        throw new Error('Invalid mapping: ' + JSON.stringify({
          generated: aGenerated,
          source: aSource,
          orginal: aOriginal,
          name: aName
        }));
      }
    };

  /**
   * Serialize the accumulated mappings in to the stream of base 64 VLQs
   * specified by the source map format.
   */
  SourceMapGenerator.prototype._serializeMappings =
    function SourceMapGenerator_serializeMappings() {
      var previousGeneratedColumn = 0;
      var previousGeneratedLine = 1;
      var previousOriginalColumn = 0;
      var previousOriginalLine = 0;
      var previousName = 0;
      var previousSource = 0;
      var result = '';
      var mapping;

      // The mappings must be guaranteed to be in sorted order before we start
      // serializing them or else the generated line numbers (which are defined
      // via the ';' separators) will be all messed up. Note: it might be more
      // performant to maintain the sorting as we insert them, rather than as we
      // serialize them, but the big O is the same either way.
      this._mappings.sort(util.compareByGeneratedPositions);

      for (var i = 0, len = this._mappings.length; i < len; i++) {
        mapping = this._mappings[i];

        if (mapping.generatedLine !== previousGeneratedLine) {
          previousGeneratedColumn = 0;
          while (mapping.generatedLine !== previousGeneratedLine) {
            result += ';';
            previousGeneratedLine++;
          }
        }
        else {
          if (i > 0) {
            if (!util.compareByGeneratedPositions(mapping, this._mappings[i - 1])) {
              continue;
            }
            result += ',';
          }
        }

        result += base64VLQ.encode(mapping.generatedColumn
                                   - previousGeneratedColumn);
        previousGeneratedColumn = mapping.generatedColumn;

        if (mapping.source) {
          result += base64VLQ.encode(this._sources.indexOf(mapping.source)
                                     - previousSource);
          previousSource = this._sources.indexOf(mapping.source);

          // lines are stored 0-based in SourceMap spec version 3
          result += base64VLQ.encode(mapping.originalLine - 1
                                     - previousOriginalLine);
          previousOriginalLine = mapping.originalLine - 1;

          result += base64VLQ.encode(mapping.originalColumn
                                     - previousOriginalColumn);
          previousOriginalColumn = mapping.originalColumn;

          if (mapping.name) {
            result += base64VLQ.encode(this._names.indexOf(mapping.name)
                                       - previousName);
            previousName = this._names.indexOf(mapping.name);
          }
        }
      }

      return result;
    };

  SourceMapGenerator.prototype._generateSourcesContent =
    function SourceMapGenerator_generateSourcesContent(aSources, aSourceRoot) {
      return aSources.map(function (source) {
        if (!this._sourcesContents) {
          return null;
        }
        if (aSourceRoot) {
          source = util.relative(aSourceRoot, source);
        }
        var key = util.toSetString(source);
        return Object.prototype.hasOwnProperty.call(this._sourcesContents,
                                                    key)
          ? this._sourcesContents[key]
          : null;
      }, this);
    };

  /**
   * Externalize the source map.
   */
  SourceMapGenerator.prototype.toJSON =
    function SourceMapGenerator_toJSON() {
      var map = {
        version: this._version,
        file: this._file,
        sources: this._sources.toArray(),
        names: this._names.toArray(),
        mappings: this._serializeMappings()
      };
      if (this._sourceRoot) {
        map.sourceRoot = this._sourceRoot;
      }
      if (this._sourcesContents) {
        map.sourcesContent = this._generateSourcesContent(map.sources, map.sourceRoot);
      }

      return map;
    };

  /**
   * Render the source map being generated to a string.
   */
  SourceMapGenerator.prototype.toString =
    function SourceMapGenerator_toString() {
      return JSON.stringify(this);
    };

  exports.SourceMapGenerator = SourceMapGenerator;

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Based on the Base 64 VLQ implementation in Closure Compiler:
 * https://code.google.com/p/closure-compiler/source/browse/trunk/src/com/google/debugging/sourcemap/Base64VLQ.java
 *
 * Copyright 2011 The Closure Compiler Authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Google Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
define('source-map/base64-vlq', ['require', 'exports', 'module' ,  'source-map/base64'], function(require, exports, module) {

  var base64 = require('./base64');

  // A single base 64 digit can contain 6 bits of data. For the base 64 variable
  // length quantities we use in the source map spec, the first bit is the sign,
  // the next four bits are the actual value, and the 6th bit is the
  // continuation bit. The continuation bit tells us whether there are more
  // digits in this value following this digit.
  //
  //   Continuation
  //   |    Sign
  //   |    |
  //   V    V
  //   101011

  var VLQ_BASE_SHIFT = 5;

  // binary: 100000
  var VLQ_BASE = 1 << VLQ_BASE_SHIFT;

  // binary: 011111
  var VLQ_BASE_MASK = VLQ_BASE - 1;

  // binary: 100000
  var VLQ_CONTINUATION_BIT = VLQ_BASE;

  /**
   * Converts from a two-complement value to a value where the sign bit is
   * is placed in the least significant bit.  For example, as decimals:
   *   1 becomes 2 (10 binary), -1 becomes 3 (11 binary)
   *   2 becomes 4 (100 binary), -2 becomes 5 (101 binary)
   */
  function toVLQSigned(aValue) {
    return aValue < 0
      ? ((-aValue) << 1) + 1
      : (aValue << 1) + 0;
  }

  /**
   * Converts to a two-complement value from a value where the sign bit is
   * is placed in the least significant bit.  For example, as decimals:
   *   2 (10 binary) becomes 1, 3 (11 binary) becomes -1
   *   4 (100 binary) becomes 2, 5 (101 binary) becomes -2
   */
  function fromVLQSigned(aValue) {
    var isNegative = (aValue & 1) === 1;
    var shifted = aValue >> 1;
    return isNegative
      ? -shifted
      : shifted;
  }

  /**
   * Returns the base 64 VLQ encoded value.
   */
  exports.encode = function base64VLQ_encode(aValue) {
    var encoded = "";
    var digit;

    var vlq = toVLQSigned(aValue);

    do {
      digit = vlq & VLQ_BASE_MASK;
      vlq >>>= VLQ_BASE_SHIFT;
      if (vlq > 0) {
        // There are still more digits in this value, so we must make sure the
        // continuation bit is marked.
        digit |= VLQ_CONTINUATION_BIT;
      }
      encoded += base64.encode(digit);
    } while (vlq > 0);

    return encoded;
  };

  /**
   * Decodes the next base 64 VLQ value from the given string and returns the
   * value and the rest of the string.
   */
  exports.decode = function base64VLQ_decode(aStr) {
    var i = 0;
    var strLen = aStr.length;
    var result = 0;
    var shift = 0;
    var continuation, digit;

    do {
      if (i >= strLen) {
        throw new Error("Expected more digits in base 64 VLQ value.");
      }
      digit = base64.decode(aStr.charAt(i++));
      continuation = !!(digit & VLQ_CONTINUATION_BIT);
      digit &= VLQ_BASE_MASK;
      result = result + (digit << shift);
      shift += VLQ_BASE_SHIFT;
    } while (continuation);

    return {
      value: fromVLQSigned(result),
      rest: aStr.slice(i)
    };
  };

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/base64', ['require', 'exports', 'module' , ], function(require, exports, module) {

  var charToIntMap = {};
  var intToCharMap = {};

  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    .split('')
    .forEach(function (ch, index) {
      charToIntMap[ch] = index;
      intToCharMap[index] = ch;
    });

  /**
   * Encode an integer in the range of 0 to 63 to a single base 64 digit.
   */
  exports.encode = function base64_encode(aNumber) {
    if (aNumber in intToCharMap) {
      return intToCharMap[aNumber];
    }
    throw new TypeError("Must be between 0 and 63: " + aNumber);
  };

  /**
   * Decode a single base 64 digit to an integer.
   */
  exports.decode = function base64_decode(aChar) {
    if (aChar in charToIntMap) {
      return charToIntMap[aChar];
    }
    throw new TypeError("Not a valid base 64 digit: " + aChar);
  };

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/util', ['require', 'exports', 'module' , ], function(require, exports, module) {

  /**
   * This is a helper function for getting values from parameter/options
   * objects.
   *
   * @param args The object we are extracting values from
   * @param name The name of the property we are getting.
   * @param defaultValue An optional value to return if the property is missing
   * from the object. If this is not specified and the property is missing, an
   * error will be thrown.
   */
  function getArg(aArgs, aName, aDefaultValue) {
    if (aName in aArgs) {
      return aArgs[aName];
    } else if (arguments.length === 3) {
      return aDefaultValue;
    } else {
      throw new Error('"' + aName + '" is a required argument.');
    }
  }
  exports.getArg = getArg;

  var urlRegexp = /([\w+\-.]+):\/\/((\w+:\w+)@)?([\w.]+)?(:(\d+))?(\S+)?/;
  var dataUrlRegexp = /^data:.+\,.+/;

  function urlParse(aUrl) {
    var match = aUrl.match(urlRegexp);
    if (!match) {
      return null;
    }
    return {
      scheme: match[1],
      auth: match[3],
      host: match[4],
      port: match[6],
      path: match[7]
    };
  }
  exports.urlParse = urlParse;

  function urlGenerate(aParsedUrl) {
    var url = aParsedUrl.scheme + "://";
    if (aParsedUrl.auth) {
      url += aParsedUrl.auth + "@"
    }
    if (aParsedUrl.host) {
      url += aParsedUrl.host;
    }
    if (aParsedUrl.port) {
      url += ":" + aParsedUrl.port
    }
    if (aParsedUrl.path) {
      url += aParsedUrl.path;
    }
    return url;
  }
  exports.urlGenerate = urlGenerate;

  function join(aRoot, aPath) {
    var url;

    if (aPath.match(urlRegexp) || aPath.match(dataUrlRegexp)) {
      return aPath;
    }

    if (aPath.charAt(0) === '/' && (url = urlParse(aRoot))) {
      url.path = aPath;
      return urlGenerate(url);
    }

    return aRoot.replace(/\/$/, '') + '/' + aPath;
  }
  exports.join = join;

  /**
   * Because behavior goes wacky when you set `__proto__` on objects, we
   * have to prefix all the strings in our set with an arbitrary character.
   *
   * See https://github.com/mozilla/source-map/pull/31 and
   * https://github.com/mozilla/source-map/issues/30
   *
   * @param String aStr
   */
  function toSetString(aStr) {
    return '$' + aStr;
  }
  exports.toSetString = toSetString;

  function fromSetString(aStr) {
    return aStr.substr(1);
  }
  exports.fromSetString = fromSetString;

  function relative(aRoot, aPath) {
    aRoot = aRoot.replace(/\/$/, '');

    var url = urlParse(aRoot);
    if (aPath.charAt(0) == "/" && url && url.path == "/") {
      return aPath.slice(1);
    }

    return aPath.indexOf(aRoot + '/') === 0
      ? aPath.substr(aRoot.length + 1)
      : aPath;
  }
  exports.relative = relative;

  function strcmp(aStr1, aStr2) {
    var s1 = aStr1 || "";
    var s2 = aStr2 || "";
    return (s1 > s2) - (s1 < s2);
  }

  /**
   * Comparator between two mappings where the original positions are compared.
   *
   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
   * mappings with the same original source/line/column, but different generated
   * line and column the same. Useful when searching for a mapping with a
   * stubbed out mapping.
   */
  function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
    var cmp;

    cmp = strcmp(mappingA.source, mappingB.source);
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.originalLine - mappingB.originalLine;
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.originalColumn - mappingB.originalColumn;
    if (cmp || onlyCompareOriginal) {
      return cmp;
    }

    cmp = strcmp(mappingA.name, mappingB.name);
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.generatedLine - mappingB.generatedLine;
    if (cmp) {
      return cmp;
    }

    return mappingA.generatedColumn - mappingB.generatedColumn;
  };
  exports.compareByOriginalPositions = compareByOriginalPositions;

  /**
   * Comparator between two mappings where the generated positions are
   * compared.
   *
   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
   * mappings with the same generated line and column, but different
   * source/name/original line and column the same. Useful when searching for a
   * mapping with a stubbed out mapping.
   */
  function compareByGeneratedPositions(mappingA, mappingB, onlyCompareGenerated) {
    var cmp;

    cmp = mappingA.generatedLine - mappingB.generatedLine;
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
    if (cmp || onlyCompareGenerated) {
      return cmp;
    }

    cmp = strcmp(mappingA.source, mappingB.source);
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.originalLine - mappingB.originalLine;
    if (cmp) {
      return cmp;
    }

    cmp = mappingA.originalColumn - mappingB.originalColumn;
    if (cmp) {
      return cmp;
    }

    return strcmp(mappingA.name, mappingB.name);
  };
  exports.compareByGeneratedPositions = compareByGeneratedPositions;

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/array-set', ['require', 'exports', 'module' ,  'source-map/util'], function(require, exports, module) {

  var util = require('./util');

  /**
   * A data structure which is a combination of an array and a set. Adding a new
   * member is O(1), testing for membership is O(1), and finding the index of an
   * element is O(1). Removing elements from the set is not supported. Only
   * strings are supported for membership.
   */
  function ArraySet() {
    this._array = [];
    this._set = {};
  }

  /**
   * Static method for creating ArraySet instances from an existing array.
   */
  ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
    var set = new ArraySet();
    for (var i = 0, len = aArray.length; i < len; i++) {
      set.add(aArray[i], aAllowDuplicates);
    }
    return set;
  };

  /**
   * Add the given string to this set.
   *
   * @param String aStr
   */
  ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
    var isDuplicate = this.has(aStr);
    var idx = this._array.length;
    if (!isDuplicate || aAllowDuplicates) {
      this._array.push(aStr);
    }
    if (!isDuplicate) {
      this._set[util.toSetString(aStr)] = idx;
    }
  };

  /**
   * Is the given string a member of this set?
   *
   * @param String aStr
   */
  ArraySet.prototype.has = function ArraySet_has(aStr) {
    return Object.prototype.hasOwnProperty.call(this._set,
                                                util.toSetString(aStr));
  };

  /**
   * What is the index of the given string in the array?
   *
   * @param String aStr
   */
  ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
    if (this.has(aStr)) {
      return this._set[util.toSetString(aStr)];
    }
    throw new Error('"' + aStr + '" is not in the set.');
  };

  /**
   * What is the element at the given index?
   *
   * @param Number aIdx
   */
  ArraySet.prototype.at = function ArraySet_at(aIdx) {
    if (aIdx >= 0 && aIdx < this._array.length) {
      return this._array[aIdx];
    }
    throw new Error('No element indexed by ' + aIdx);
  };

  /**
   * Returns the array representation of this set (which has the proper indices
   * indicated by indexOf). Note that this is a copy of the internal array used
   * for storing the members so that no one can mess with internal state.
   */
  ArraySet.prototype.toArray = function ArraySet_toArray() {
    return this._array.slice();
  };

  exports.ArraySet = ArraySet;

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/source-map-consumer', ['require', 'exports', 'module' ,  'source-map/util', 'source-map/binary-search', 'source-map/array-set', 'source-map/base64-vlq'], function(require, exports, module) {

  var util = require('./util');
  var binarySearch = require('./binary-search');
  var ArraySet = require('./array-set').ArraySet;
  var base64VLQ = require('./base64-vlq');

  /**
   * A SourceMapConsumer instance represents a parsed source map which we can
   * query for information about the original file positions by giving it a file
   * position in the generated source.
   *
   * The only parameter is the raw source map (either as a JSON string, or
   * already parsed to an object). According to the spec, source maps have the
   * following attributes:
   *
   *   - version: Which version of the source map spec this map is following.
   *   - sources: An array of URLs to the original source files.
   *   - names: An array of identifiers which can be referrenced by individual mappings.
   *   - sourceRoot: Optional. The URL root from which all sources are relative.
   *   - sourcesContent: Optional. An array of contents of the original source files.
   *   - mappings: A string of base64 VLQs which contain the actual mappings.
   *   - file: The generated file this source map is associated with.
   *
   * Here is an example source map, taken from the source map spec[0]:
   *
   *     {
   *       version : 3,
   *       file: "out.js",
   *       sourceRoot : "",
   *       sources: ["foo.js", "bar.js"],
   *       names: ["src", "maps", "are", "fun"],
   *       mappings: "AA,AB;;ABCDE;"
   *     }
   *
   * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit?pli=1#
   */
  function SourceMapConsumer(aSourceMap) {
    var sourceMap = aSourceMap;
    if (typeof aSourceMap === 'string') {
      sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
    }

    var version = util.getArg(sourceMap, 'version');
    var sources = util.getArg(sourceMap, 'sources');
    var names = util.getArg(sourceMap, 'names');
    var sourceRoot = util.getArg(sourceMap, 'sourceRoot', null);
    var sourcesContent = util.getArg(sourceMap, 'sourcesContent', null);
    var mappings = util.getArg(sourceMap, 'mappings');
    var file = util.getArg(sourceMap, 'file', null);

    if (version !== this._version) {
      throw new Error('Unsupported version: ' + version);
    }

    // Pass `true` below to allow duplicate names and sources. While source maps
    // are intended to be compressed and deduplicated, the TypeScript compiler
    // sometimes generates source maps with duplicates in them. See Github issue
    // #72 and bugzil.la/889492.
    this._names = ArraySet.fromArray(names, true);
    this._sources = ArraySet.fromArray(sources, true);
    this.sourceRoot = sourceRoot;
    this.sourcesContent = sourcesContent;
    this.file = file;

    // `this._generatedMappings` and `this._originalMappings` hold the parsed
    // mapping coordinates from the source map's "mappings" attribute. Each
    // object in the array is of the form
    //
    //     {
    //       generatedLine: The line number in the generated code,
    //       generatedColumn: The column number in the generated code,
    //       source: The path to the original source file that generated this
    //               chunk of code,
    //       originalLine: The line number in the original source that
    //                     corresponds to this chunk of generated code,
    //       originalColumn: The column number in the original source that
    //                       corresponds to this chunk of generated code,
    //       name: The name of the original symbol which generated this chunk of
    //             code.
    //     }
    //
    // All properties except for `generatedLine` and `generatedColumn` can be
    // `null`.
    //
    // `this._generatedMappings` is ordered by the generated positions.
    //
    // `this._originalMappings` is ordered by the original positions.
    this._generatedMappings = [];
    this._originalMappings = [];
    this._parseMappings(mappings, sourceRoot);
  }

  /**
   * Create a SourceMapConsumer from a SourceMapGenerator.
   *
   * @param SourceMapGenerator aSourceMap
   *        The source map that will be consumed.
   * @returns SourceMapConsumer
   */
  SourceMapConsumer.fromSourceMap =
    function SourceMapConsumer_fromSourceMap(aSourceMap) {
      var smc = Object.create(SourceMapConsumer.prototype);

      smc._names = ArraySet.fromArray(aSourceMap._names.toArray(), true);
      smc._sources = ArraySet.fromArray(aSourceMap._sources.toArray(), true);
      smc.sourceRoot = aSourceMap._sourceRoot;
      smc.sourcesContent = aSourceMap._generateSourcesContent(smc._sources.toArray(),
                                                              smc.sourceRoot);
      smc.file = aSourceMap._file;

      smc._generatedMappings = aSourceMap._mappings.slice()
        .sort(util.compareByGeneratedPositions);
      smc._originalMappings = aSourceMap._mappings.slice()
        .sort(util.compareByOriginalPositions);

      return smc;
    };

  /**
   * The version of the source mapping spec that we are consuming.
   */
  SourceMapConsumer.prototype._version = 3;

  /**
   * The list of original sources.
   */
  Object.defineProperty(SourceMapConsumer.prototype, 'sources', {
    get: function () {
      return this._sources.toArray().map(function (s) {
        return this.sourceRoot ? util.join(this.sourceRoot, s) : s;
      }, this);
    }
  });

  /**
   * Parse the mappings in a string in to a data structure which we can easily
   * query (an ordered list in this._generatedMappings).
   */
  SourceMapConsumer.prototype._parseMappings =
    function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
      var generatedLine = 1;
      var previousGeneratedColumn = 0;
      var previousOriginalLine = 0;
      var previousOriginalColumn = 0;
      var previousSource = 0;
      var previousName = 0;
      var mappingSeparator = /^[,;]/;
      var str = aStr;
      var mapping;
      var temp;

      while (str.length > 0) {
        if (str.charAt(0) === ';') {
          generatedLine++;
          str = str.slice(1);
          previousGeneratedColumn = 0;
        }
        else if (str.charAt(0) === ',') {
          str = str.slice(1);
        }
        else {
          mapping = {};
          mapping.generatedLine = generatedLine;

          // Generated column.
          temp = base64VLQ.decode(str);
          mapping.generatedColumn = previousGeneratedColumn + temp.value;
          previousGeneratedColumn = mapping.generatedColumn;
          str = temp.rest;

          if (str.length > 0 && !mappingSeparator.test(str.charAt(0))) {
            // Original source.
            temp = base64VLQ.decode(str);
            mapping.source = this._sources.at(previousSource + temp.value);
            previousSource += temp.value;
            str = temp.rest;
            if (str.length === 0 || mappingSeparator.test(str.charAt(0))) {
              throw new Error('Found a source, but no line and column');
            }

            // Original line.
            temp = base64VLQ.decode(str);
            mapping.originalLine = previousOriginalLine + temp.value;
            previousOriginalLine = mapping.originalLine;
            // Lines are stored 0-based
            mapping.originalLine += 1;
            str = temp.rest;
            if (str.length === 0 || mappingSeparator.test(str.charAt(0))) {
              throw new Error('Found a source and line, but no column');
            }

            // Original column.
            temp = base64VLQ.decode(str);
            mapping.originalColumn = previousOriginalColumn + temp.value;
            previousOriginalColumn = mapping.originalColumn;
            str = temp.rest;

            if (str.length > 0 && !mappingSeparator.test(str.charAt(0))) {
              // Original name.
              temp = base64VLQ.decode(str);
              mapping.name = this._names.at(previousName + temp.value);
              previousName += temp.value;
              str = temp.rest;
            }
          }

          this._generatedMappings.push(mapping);
          if (typeof mapping.originalLine === 'number') {
            this._originalMappings.push(mapping);
          }
        }
      }

      this._originalMappings.sort(util.compareByOriginalPositions);
    };

  /**
   * Find the mapping that best matches the hypothetical "needle" mapping that
   * we are searching for in the given "haystack" of mappings.
   */
  SourceMapConsumer.prototype._findMapping =
    function SourceMapConsumer_findMapping(aNeedle, aMappings, aLineName,
                                           aColumnName, aComparator) {
      // To return the position we are searching for, we must first find the
      // mapping for the given position and then return the opposite position it
      // points to. Because the mappings are sorted, we can use binary search to
      // find the best mapping.

      if (aNeedle[aLineName] <= 0) {
        throw new TypeError('Line must be greater than or equal to 1, got '
                            + aNeedle[aLineName]);
      }
      if (aNeedle[aColumnName] < 0) {
        throw new TypeError('Column must be greater than or equal to 0, got '
                            + aNeedle[aColumnName]);
      }

      return binarySearch.search(aNeedle, aMappings, aComparator);
    };

  /**
   * Returns the original source, line, and column information for the generated
   * source's line and column positions provided. The only argument is an object
   * with the following properties:
   *
   *   - line: The line number in the generated source.
   *   - column: The column number in the generated source.
   *
   * and an object is returned with the following properties:
   *
   *   - source: The original source file, or null.
   *   - line: The line number in the original source, or null.
   *   - column: The column number in the original source, or null.
   *   - name: The original identifier, or null.
   */
  SourceMapConsumer.prototype.originalPositionFor =
    function SourceMapConsumer_originalPositionFor(aArgs) {
      var needle = {
        generatedLine: util.getArg(aArgs, 'line'),
        generatedColumn: util.getArg(aArgs, 'column')
      };

      var mapping = this._findMapping(needle,
                                      this._generatedMappings,
                                      "generatedLine",
                                      "generatedColumn",
                                      util.compareByGeneratedPositions);

      if (mapping) {
        var source = util.getArg(mapping, 'source', null);
        if (source && this.sourceRoot) {
          source = util.join(this.sourceRoot, source);
        }
        return {
          source: source,
          line: util.getArg(mapping, 'originalLine', null),
          column: util.getArg(mapping, 'originalColumn', null),
          name: util.getArg(mapping, 'name', null)
        };
      }

      return {
        source: null,
        line: null,
        column: null,
        name: null
      };
    };

  /**
   * Returns the original source content. The only argument is the url of the
   * original source file. Returns null if no original source content is
   * availible.
   */
  SourceMapConsumer.prototype.sourceContentFor =
    function SourceMapConsumer_sourceContentFor(aSource) {
      if (!this.sourcesContent) {
        return null;
      }

      if (this.sourceRoot) {
        aSource = util.relative(this.sourceRoot, aSource);
      }

      if (this._sources.has(aSource)) {
        return this.sourcesContent[this._sources.indexOf(aSource)];
      }

      var url;
      if (this.sourceRoot
          && (url = util.urlParse(this.sourceRoot))) {
        // XXX: file:// URIs and absolute paths lead to unexpected behavior for
        // many users. We can help them out when they expect file:// URIs to
        // behave like it would if they were running a local HTTP server. See
        // https://bugzilla.mozilla.org/show_bug.cgi?id=885597.
        var fileUriAbsPath = aSource.replace(/^file:\/\//, "");
        if (url.scheme == "file"
            && this._sources.has(fileUriAbsPath)) {
          return this.sourcesContent[this._sources.indexOf(fileUriAbsPath)]
        }

        if ((!url.path || url.path == "/")
            && this._sources.has("/" + aSource)) {
          return this.sourcesContent[this._sources.indexOf("/" + aSource)];
        }
      }

      throw new Error('"' + aSource + '" is not in the SourceMap.');
    };

  /**
   * Returns the generated line and column information for the original source,
   * line, and column positions provided. The only argument is an object with
   * the following properties:
   *
   *   - source: The filename of the original source.
   *   - line: The line number in the original source.
   *   - column: The column number in the original source.
   *
   * and an object is returned with the following properties:
   *
   *   - line: The line number in the generated source, or null.
   *   - column: The column number in the generated source, or null.
   */
  SourceMapConsumer.prototype.generatedPositionFor =
    function SourceMapConsumer_generatedPositionFor(aArgs) {
      var needle = {
        source: util.getArg(aArgs, 'source'),
        originalLine: util.getArg(aArgs, 'line'),
        originalColumn: util.getArg(aArgs, 'column')
      };

      if (this.sourceRoot) {
        needle.source = util.relative(this.sourceRoot, needle.source);
      }

      var mapping = this._findMapping(needle,
                                      this._originalMappings,
                                      "originalLine",
                                      "originalColumn",
                                      util.compareByOriginalPositions);

      if (mapping) {
        return {
          line: util.getArg(mapping, 'generatedLine', null),
          column: util.getArg(mapping, 'generatedColumn', null)
        };
      }

      return {
        line: null,
        column: null
      };
    };

  SourceMapConsumer.GENERATED_ORDER = 1;
  SourceMapConsumer.ORIGINAL_ORDER = 2;

  /**
   * Iterate over each mapping between an original source/line/column and a
   * generated line/column in this source map.
   *
   * @param Function aCallback
   *        The function that is called with each mapping.
   * @param Object aContext
   *        Optional. If specified, this object will be the value of `this` every
   *        time that `aCallback` is called.
   * @param aOrder
   *        Either `SourceMapConsumer.GENERATED_ORDER` or
   *        `SourceMapConsumer.ORIGINAL_ORDER`. Specifies whether you want to
   *        iterate over the mappings sorted by the generated file's line/column
   *        order or the original's source/line/column order, respectively. Defaults to
   *        `SourceMapConsumer.GENERATED_ORDER`.
   */
  SourceMapConsumer.prototype.eachMapping =
    function SourceMapConsumer_eachMapping(aCallback, aContext, aOrder) {
      var context = aContext || null;
      var order = aOrder || SourceMapConsumer.GENERATED_ORDER;

      var mappings;
      switch (order) {
      case SourceMapConsumer.GENERATED_ORDER:
        mappings = this._generatedMappings;
        break;
      case SourceMapConsumer.ORIGINAL_ORDER:
        mappings = this._originalMappings;
        break;
      default:
        throw new Error("Unknown order of iteration.");
      }

      var sourceRoot = this.sourceRoot;
      mappings.map(function (mapping) {
        var source = mapping.source;
        if (source && sourceRoot) {
          source = util.join(sourceRoot, source);
        }
        return {
          source: source,
          generatedLine: mapping.generatedLine,
          generatedColumn: mapping.generatedColumn,
          originalLine: mapping.originalLine,
          originalColumn: mapping.originalColumn,
          name: mapping.name
        };
      }).forEach(aCallback, context);
    };

  exports.SourceMapConsumer = SourceMapConsumer;

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/binary-search', ['require', 'exports', 'module' , ], function(require, exports, module) {

  /**
   * Recursive implementation of binary search.
   *
   * @param aLow Indices here and lower do not contain the needle.
   * @param aHigh Indices here and higher do not contain the needle.
   * @param aNeedle The element being searched for.
   * @param aHaystack The non-empty array being searched.
   * @param aCompare Function which takes two elements and returns -1, 0, or 1.
   */
  function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare) {
    // This function terminates when one of the following is true:
    //
    //   1. We find the exact element we are looking for.
    //
    //   2. We did not find the exact element, but we can return the next
    //      closest element that is less than that element.
    //
    //   3. We did not find the exact element, and there is no next-closest
    //      element which is less than the one we are searching for, so we
    //      return null.
    var mid = Math.floor((aHigh - aLow) / 2) + aLow;
    var cmp = aCompare(aNeedle, aHaystack[mid], true);
    if (cmp === 0) {
      // Found the element we are looking for.
      return aHaystack[mid];
    }
    else if (cmp > 0) {
      // aHaystack[mid] is greater than our needle.
      if (aHigh - mid > 1) {
        // The element is in the upper half.
        return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare);
      }
      // We did not find an exact match, return the next closest one
      // (termination case 2).
      return aHaystack[mid];
    }
    else {
      // aHaystack[mid] is less than our needle.
      if (mid - aLow > 1) {
        // The element is in the lower half.
        return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare);
      }
      // The exact needle element was not found in this haystack. Determine if
      // we are in termination case (2) or (3) and return the appropriate thing.
      return aLow < 0
        ? null
        : aHaystack[aLow];
    }
  }

  /**
   * This is an implementation of binary search which will always try and return
   * the next lowest value checked if there is no exact hit. This is because
   * mappings between original and generated line/col pairs are single points,
   * and there is an implicit region between each of them, so a miss just means
   * that you aren't on the very start of a region.
   *
   * @param aNeedle The element you are looking for.
   * @param aHaystack The array that is being searched.
   * @param aCompare A function which takes the needle and an element in the
   *     array and returns -1, 0, or 1 depending on whether the needle is less
   *     than, equal to, or greater than the element, respectively.
   */
  exports.search = function search(aNeedle, aHaystack, aCompare) {
    return aHaystack.length > 0
      ? recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack, aCompare)
      : null;
  };

});
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
define('source-map/source-node', ['require', 'exports', 'module' ,  'source-map/source-map-generator', 'source-map/util'], function(require, exports, module) {

  var SourceMapGenerator = require('./source-map-generator').SourceMapGenerator;
  var util = require('./util');

  /**
   * SourceNodes provide a way to abstract over interpolating/concatenating
   * snippets of generated JavaScript source code while maintaining the line and
   * column information associated with the original source code.
   *
   * @param aLine The original line number.
   * @param aColumn The original column number.
   * @param aSource The original source's filename.
   * @param aChunks Optional. An array of strings which are snippets of
   *        generated JS, or other SourceNodes.
   * @param aName The original identifier.
   */
  function SourceNode(aLine, aColumn, aSource, aChunks, aName) {
    this.children = [];
    this.sourceContents = {};
    this.line = aLine === undefined ? null : aLine;
    this.column = aColumn === undefined ? null : aColumn;
    this.source = aSource === undefined ? null : aSource;
    this.name = aName === undefined ? null : aName;
    if (aChunks != null) this.add(aChunks);
  }

  /**
   * Creates a SourceNode from generated code and a SourceMapConsumer.
   *
   * @param aGeneratedCode The generated code
   * @param aSourceMapConsumer The SourceMap for the generated code
   */
  SourceNode.fromStringWithSourceMap =
    function SourceNode_fromStringWithSourceMap(aGeneratedCode, aSourceMapConsumer) {
      // The SourceNode we want to fill with the generated code
      // and the SourceMap
      var node = new SourceNode();

      // The generated code
      // Processed fragments are removed from this array.
      var remainingLines = aGeneratedCode.split('\n');

      // We need to remember the position of "remainingLines"
      var lastGeneratedLine = 1, lastGeneratedColumn = 0;

      // The generate SourceNodes we need a code range.
      // To extract it current and last mapping is used.
      // Here we store the last mapping.
      var lastMapping = null;

      aSourceMapConsumer.eachMapping(function (mapping) {
        if (lastMapping === null) {
          // We add the generated code until the first mapping
          // to the SourceNode without any mapping.
          // Each line is added as separate string.
          while (lastGeneratedLine < mapping.generatedLine) {
            node.add(remainingLines.shift() + "\n");
            lastGeneratedLine++;
          }
          if (lastGeneratedColumn < mapping.generatedColumn) {
            var nextLine = remainingLines[0];
            node.add(nextLine.substr(0, mapping.generatedColumn));
            remainingLines[0] = nextLine.substr(mapping.generatedColumn);
            lastGeneratedColumn = mapping.generatedColumn;
          }
        } else {
          // We add the code from "lastMapping" to "mapping":
          // First check if there is a new line in between.
          if (lastGeneratedLine < mapping.generatedLine) {
            var code = "";
            // Associate full lines with "lastMapping"
            do {
              code += remainingLines.shift() + "\n";
              lastGeneratedLine++;
              lastGeneratedColumn = 0;
            } while (lastGeneratedLine < mapping.generatedLine);
            // When we reached the correct line, we add code until we
            // reach the correct column too.
            if (lastGeneratedColumn < mapping.generatedColumn) {
              var nextLine = remainingLines[0];
              code += nextLine.substr(0, mapping.generatedColumn);
              remainingLines[0] = nextLine.substr(mapping.generatedColumn);
              lastGeneratedColumn = mapping.generatedColumn;
            }
            // Create the SourceNode.
            addMappingWithCode(lastMapping, code);
          } else {
            // There is no new line in between.
            // Associate the code between "lastGeneratedColumn" and
            // "mapping.generatedColumn" with "lastMapping"
            var nextLine = remainingLines[0];
            var code = nextLine.substr(0, mapping.generatedColumn -
                                          lastGeneratedColumn);
            remainingLines[0] = nextLine.substr(mapping.generatedColumn -
                                                lastGeneratedColumn);
            lastGeneratedColumn = mapping.generatedColumn;
            addMappingWithCode(lastMapping, code);
          }
        }
        lastMapping = mapping;
      }, this);
      // We have processed all mappings.
      // Associate the remaining code in the current line with "lastMapping"
      // and add the remaining lines without any mapping
      addMappingWithCode(lastMapping, remainingLines.join("\n"));

      // Copy sourcesContent into SourceNode
      aSourceMapConsumer.sources.forEach(function (sourceFile) {
        var content = aSourceMapConsumer.sourceContentFor(sourceFile);
        if (content) {
          node.setSourceContent(sourceFile, content);
        }
      });

      return node;

      function addMappingWithCode(mapping, code) {
        if (mapping === null || mapping.source === undefined) {
          node.add(code);
        } else {
          node.add(new SourceNode(mapping.originalLine,
                                  mapping.originalColumn,
                                  mapping.source,
                                  code,
                                  mapping.name));
        }
      }
    };

  /**
   * Add a chunk of generated JS to this source node.
   *
   * @param aChunk A string snippet of generated JS code, another instance of
   *        SourceNode, or an array where each member is one of those things.
   */
  SourceNode.prototype.add = function SourceNode_add(aChunk) {
    if (Array.isArray(aChunk)) {
      aChunk.forEach(function (chunk) {
        this.add(chunk);
      }, this);
    }
    else if (aChunk instanceof SourceNode || typeof aChunk === "string") {
      if (aChunk) {
        this.children.push(aChunk);
      }
    }
    else {
      throw new TypeError(
        "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
      );
    }
    return this;
  };

  /**
   * Add a chunk of generated JS to the beginning of this source node.
   *
   * @param aChunk A string snippet of generated JS code, another instance of
   *        SourceNode, or an array where each member is one of those things.
   */
  SourceNode.prototype.prepend = function SourceNode_prepend(aChunk) {
    if (Array.isArray(aChunk)) {
      for (var i = aChunk.length-1; i >= 0; i--) {
        this.prepend(aChunk[i]);
      }
    }
    else if (aChunk instanceof SourceNode || typeof aChunk === "string") {
      this.children.unshift(aChunk);
    }
    else {
      throw new TypeError(
        "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
      );
    }
    return this;
  };

  /**
   * Walk over the tree of JS snippets in this node and its children. The
   * walking function is called once for each snippet of JS and is passed that
   * snippet and the its original associated source's line/column location.
   *
   * @param aFn The traversal function.
   */
  SourceNode.prototype.walk = function SourceNode_walk(aFn) {
    var chunk;
    for (var i = 0, len = this.children.length; i < len; i++) {
      chunk = this.children[i];
      if (chunk instanceof SourceNode) {
        chunk.walk(aFn);
      }
      else {
        if (chunk !== '') {
          aFn(chunk, { source: this.source,
                       line: this.line,
                       column: this.column,
                       name: this.name });
        }
      }
    }
  };

  /**
   * Like `String.prototype.join` except for SourceNodes. Inserts `aStr` between
   * each of `this.children`.
   *
   * @param aSep The separator.
   */
  SourceNode.prototype.join = function SourceNode_join(aSep) {
    var newChildren;
    var i;
    var len = this.children.length;
    if (len > 0) {
      newChildren = [];
      for (i = 0; i < len-1; i++) {
        newChildren.push(this.children[i]);
        newChildren.push(aSep);
      }
      newChildren.push(this.children[i]);
      this.children = newChildren;
    }
    return this;
  };

  /**
   * Call String.prototype.replace on the very right-most source snippet. Useful
   * for trimming whitespace from the end of a source node, etc.
   *
   * @param aPattern The pattern to replace.
   * @param aReplacement The thing to replace the pattern with.
   */
  SourceNode.prototype.replaceRight = function SourceNode_replaceRight(aPattern, aReplacement) {
    var lastChild = this.children[this.children.length - 1];
    if (lastChild instanceof SourceNode) {
      lastChild.replaceRight(aPattern, aReplacement);
    }
    else if (typeof lastChild === 'string') {
      this.children[this.children.length - 1] = lastChild.replace(aPattern, aReplacement);
    }
    else {
      this.children.push(''.replace(aPattern, aReplacement));
    }
    return this;
  };

  /**
   * Set the source content for a source file. This will be added to the SourceMapGenerator
   * in the sourcesContent field.
   *
   * @param aSourceFile The filename of the source file
   * @param aSourceContent The content of the source file
   */
  SourceNode.prototype.setSourceContent =
    function SourceNode_setSourceContent(aSourceFile, aSourceContent) {
      this.sourceContents[util.toSetString(aSourceFile)] = aSourceContent;
    };

  /**
   * Walk over the tree of SourceNodes. The walking function is called for each
   * source file content and is passed the filename and source content.
   *
   * @param aFn The traversal function.
   */
  SourceNode.prototype.walkSourceContents =
    function SourceNode_walkSourceContents(aFn) {
      for (var i = 0, len = this.children.length; i < len; i++) {
        if (this.children[i] instanceof SourceNode) {
          this.children[i].walkSourceContents(aFn);
        }
      }

      var sources = Object.keys(this.sourceContents);
      for (var i = 0, len = sources.length; i < len; i++) {
        aFn(util.fromSetString(sources[i]), this.sourceContents[sources[i]]);
      }
    };

  /**
   * Return the string representation of this source node. Walks over the tree
   * and concatenates all the various snippets together to one string.
   */
  SourceNode.prototype.toString = function SourceNode_toString() {
    var str = "";
    this.walk(function (chunk) {
      str += chunk;
    });
    return str;
  };

  /**
   * Returns the string representation of this source node along with a source
   * map.
   */
  SourceNode.prototype.toStringWithSourceMap = function SourceNode_toStringWithSourceMap(aArgs) {
    var generated = {
      code: "",
      line: 1,
      column: 0
    };
    var map = new SourceMapGenerator(aArgs);
    var sourceMappingActive = false;
    var lastOriginalSource = null;
    var lastOriginalLine = null;
    var lastOriginalColumn = null;
    var lastOriginalName = null;
    this.walk(function (chunk, original) {
      generated.code += chunk;
      if (original.source !== null
          && original.line !== null
          && original.column !== null) {
        if(lastOriginalSource !== original.source
           || lastOriginalLine !== original.line
           || lastOriginalColumn !== original.column
           || lastOriginalName !== original.name) {
          map.addMapping({
            source: original.source,
            original: {
              line: original.line,
              column: original.column
            },
            generated: {
              line: generated.line,
              column: generated.column
            },
            name: original.name
          });
        }
        lastOriginalSource = original.source;
        lastOriginalLine = original.line;
        lastOriginalColumn = original.column;
        lastOriginalName = original.name;
        sourceMappingActive = true;
      } else if (sourceMappingActive) {
        map.addMapping({
          generated: {
            line: generated.line,
            column: generated.column
          }
        });
        lastOriginalSource = null;
        sourceMappingActive = false;
      }
      chunk.split('').forEach(function (ch) {
        if (ch === '\n') {
          generated.line++;
          generated.column = 0;
        } else {
          generated.column++;
        }
      });
    });
    this.walkSourceContents(function (sourceFile, sourceContent) {
      map.setSourceContent(sourceFile, sourceContent);
    });

    return { code: generated.code, map: map };
  };

  exports.SourceNode = SourceNode;

});
/* -*- Mode: js; js-indent-level: 2; -*- */
///////////////////////////////////////////////////////////////////////////////

window.sourceMap = {
  SourceMapConsumer: require('source-map/source-map-consumer').SourceMapConsumer,
  SourceMapGenerator: require('source-map/source-map-generator').SourceMapGenerator,
  SourceNode: require('source-map/source-node').SourceNode
};
// Generated by CommonJS Everywhere 0.8.1
(function (global) {
  function require(file, parentModule) {
    if ({}.hasOwnProperty.call(require.cache, file))
      return require.cache[file];
    var resolved = require.resolve(file);
    if (!resolved)
      throw new Error('Failed to resolve module ' + file);
    var module$ = {
        id: file,
        require: require,
        filename: file,
        exports: {},
        loaded: false,
        parent: parentModule,
        children: []
      };
    if (parentModule)
      parentModule.children.push(module$);
    var dirname = file.slice(0, file.lastIndexOf('/') + 1);
    require.cache[file] = module$.exports;
    resolved.call(module$.exports, module$, module$.exports, dirname, file);
    module$.loaded = true;
    return require.cache[file] = module$.exports;
  }
  require.modules = {};
  require.cache = {};
  require.resolve = function (file) {
    return {}.hasOwnProperty.call(require.modules, file) ? require.modules[file] : void 0;
  };
  require.define = function (file, fn) {
    require.modules[file] = fn;
  };
  var process = function () {
      var cwd = '/';
      return {
        title: 'browser',
        version: 'v0.10.5',
        browser: true,
        env: {},
        argv: [],
        nextTick: global.setImmediate || function (fn) {
          setTimeout(fn, 0);
        },
        cwd: function () {
          return cwd;
        },
        chdir: function (dir) {
          cwd = dir;
        }
      };
    }();
  require.define('/tools/entry-point.js', function (module, exports, __dirname, __filename) {
    (function () {
      'use strict';
      global.escodegen = require('/escodegen.js', module);
      escodegen.browser = true;
    }());
  });
  require.define('/escodegen.js', function (module, exports, __dirname, __filename) {
    (function () {
      'use strict';
      var Syntax, Precedence, BinaryPrecedence, Regex, SourceNode, estraverse, isArray, base, indent, json, renumber, hexadecimal, quotes, escapeless, newline, space, parentheses, semicolons, safeConcatenation, directive, extra, parse, sourceMap, FORMAT_MINIFY, FORMAT_DEFAULTS;
      estraverse = require('/node_modules/estraverse/estraverse.js', module);
      Syntax = {
        AssignmentExpression: 'AssignmentExpression',
        ArrayExpression: 'ArrayExpression',
        ArrayPattern: 'ArrayPattern',
        ArrowFunctionExpression: 'ArrowFunctionExpression',
        BlockStatement: 'BlockStatement',
        BinaryExpression: 'BinaryExpression',
        BreakStatement: 'BreakStatement',
        CallExpression: 'CallExpression',
        CatchClause: 'CatchClause',
        ComprehensionBlock: 'ComprehensionBlock',
        ComprehensionExpression: 'ComprehensionExpression',
        ConditionalExpression: 'ConditionalExpression',
        ContinueStatement: 'ContinueStatement',
        DirectiveStatement: 'DirectiveStatement',
        DoWhileStatement: 'DoWhileStatement',
        DebuggerStatement: 'DebuggerStatement',
        EmptyStatement: 'EmptyStatement',
        ExpressionStatement: 'ExpressionStatement',
        ForStatement: 'ForStatement',
        ForInStatement: 'ForInStatement',
        FunctionDeclaration: 'FunctionDeclaration',
        FunctionExpression: 'FunctionExpression',
        Identifier: 'Identifier',
        IfStatement: 'IfStatement',
        Literal: 'Literal',
        LabeledStatement: 'LabeledStatement',
        LogicalExpression: 'LogicalExpression',
        MemberExpression: 'MemberExpression',
        NewExpression: 'NewExpression',
        ObjectExpression: 'ObjectExpression',
        ObjectPattern: 'ObjectPattern',
        Program: 'Program',
        Property: 'Property',
        ReturnStatement: 'ReturnStatement',
        SequenceExpression: 'SequenceExpression',
        SwitchStatement: 'SwitchStatement',
        SwitchCase: 'SwitchCase',
        ThisExpression: 'ThisExpression',
        ThrowStatement: 'ThrowStatement',
        TryStatement: 'TryStatement',
        UnaryExpression: 'UnaryExpression',
        UpdateExpression: 'UpdateExpression',
        VariableDeclaration: 'VariableDeclaration',
        VariableDeclarator: 'VariableDeclarator',
        WhileStatement: 'WhileStatement',
        WithStatement: 'WithStatement',
        YieldExpression: 'YieldExpression'
      };
      Precedence = {
        Sequence: 0,
        Assignment: 1,
        Conditional: 2,
        ArrowFunction: 2,
        LogicalOR: 3,
        LogicalAND: 4,
        BitwiseOR: 5,
        BitwiseXOR: 6,
        BitwiseAND: 7,
        Equality: 8,
        Relational: 9,
        BitwiseSHIFT: 10,
        Additive: 11,
        Multiplicative: 12,
        Unary: 13,
        Postfix: 14,
        Call: 15,
        New: 16,
        Member: 17,
        Primary: 18
      };
      BinaryPrecedence = {
        '||': Precedence.LogicalOR,
        '&&': Precedence.LogicalAND,
        '|': Precedence.BitwiseOR,
        '^': Precedence.BitwiseXOR,
        '&': Precedence.BitwiseAND,
        '==': Precedence.Equality,
        '!=': Precedence.Equality,
        '===': Precedence.Equality,
        '!==': Precedence.Equality,
        'is': Precedence.Equality,
        'isnt': Precedence.Equality,
        '<': Precedence.Relational,
        '>': Precedence.Relational,
        '<=': Precedence.Relational,
        '>=': Precedence.Relational,
        'in': Precedence.Relational,
        'instanceof': Precedence.Relational,
        '<<': Precedence.BitwiseSHIFT,
        '>>': Precedence.BitwiseSHIFT,
        '>>>': Precedence.BitwiseSHIFT,
        '+': Precedence.Additive,
        '-': Precedence.Additive,
        '*': Precedence.Multiplicative,
        '%': Precedence.Multiplicative,
        '/': Precedence.Multiplicative
      };
      Regex = { NonAsciiIdentifierPart: new RegExp('[\xaa\xb5\xba\xc0-\xd6\xd8-\xf6\xf8-\u02c1\u02c6-\u02d1\u02e0-\u02e4\u02ec\u02ee\u0300-\u0374\u0376\u0377\u037a-\u037d\u0386\u0388-\u038a\u038c\u038e-\u03a1\u03a3-\u03f5\u03f7-\u0481\u0483-\u0487\u048a-\u0527\u0531-\u0556\u0559\u0561-\u0587\u0591-\u05bd\u05bf\u05c1\u05c2\u05c4\u05c5\u05c7\u05d0-\u05ea\u05f0-\u05f2\u0610-\u061a\u0620-\u0669\u066e-\u06d3\u06d5-\u06dc\u06df-\u06e8\u06ea-\u06fc\u06ff\u0710-\u074a\u074d-\u07b1\u07c0-\u07f5\u07fa\u0800-\u082d\u0840-\u085b\u08a0\u08a2-\u08ac\u08e4-\u08fe\u0900-\u0963\u0966-\u096f\u0971-\u0977\u0979-\u097f\u0981-\u0983\u0985-\u098c\u098f\u0990\u0993-\u09a8\u09aa-\u09b0\u09b2\u09b6-\u09b9\u09bc-\u09c4\u09c7\u09c8\u09cb-\u09ce\u09d7\u09dc\u09dd\u09df-\u09e3\u09e6-\u09f1\u0a01-\u0a03\u0a05-\u0a0a\u0a0f\u0a10\u0a13-\u0a28\u0a2a-\u0a30\u0a32\u0a33\u0a35\u0a36\u0a38\u0a39\u0a3c\u0a3e-\u0a42\u0a47\u0a48\u0a4b-\u0a4d\u0a51\u0a59-\u0a5c\u0a5e\u0a66-\u0a75\u0a81-\u0a83\u0a85-\u0a8d\u0a8f-\u0a91\u0a93-\u0aa8\u0aaa-\u0ab0\u0ab2\u0ab3\u0ab5-\u0ab9\u0abc-\u0ac5\u0ac7-\u0ac9\u0acb-\u0acd\u0ad0\u0ae0-\u0ae3\u0ae6-\u0aef\u0b01-\u0b03\u0b05-\u0b0c\u0b0f\u0b10\u0b13-\u0b28\u0b2a-\u0b30\u0b32\u0b33\u0b35-\u0b39\u0b3c-\u0b44\u0b47\u0b48\u0b4b-\u0b4d\u0b56\u0b57\u0b5c\u0b5d\u0b5f-\u0b63\u0b66-\u0b6f\u0b71\u0b82\u0b83\u0b85-\u0b8a\u0b8e-\u0b90\u0b92-\u0b95\u0b99\u0b9a\u0b9c\u0b9e\u0b9f\u0ba3\u0ba4\u0ba8-\u0baa\u0bae-\u0bb9\u0bbe-\u0bc2\u0bc6-\u0bc8\u0bca-\u0bcd\u0bd0\u0bd7\u0be6-\u0bef\u0c01-\u0c03\u0c05-\u0c0c\u0c0e-\u0c10\u0c12-\u0c28\u0c2a-\u0c33\u0c35-\u0c39\u0c3d-\u0c44\u0c46-\u0c48\u0c4a-\u0c4d\u0c55\u0c56\u0c58\u0c59\u0c60-\u0c63\u0c66-\u0c6f\u0c82\u0c83\u0c85-\u0c8c\u0c8e-\u0c90\u0c92-\u0ca8\u0caa-\u0cb3\u0cb5-\u0cb9\u0cbc-\u0cc4\u0cc6-\u0cc8\u0cca-\u0ccd\u0cd5\u0cd6\u0cde\u0ce0-\u0ce3\u0ce6-\u0cef\u0cf1\u0cf2\u0d02\u0d03\u0d05-\u0d0c\u0d0e-\u0d10\u0d12-\u0d3a\u0d3d-\u0d44\u0d46-\u0d48\u0d4a-\u0d4e\u0d57\u0d60-\u0d63\u0d66-\u0d6f\u0d7a-\u0d7f\u0d82\u0d83\u0d85-\u0d96\u0d9a-\u0db1\u0db3-\u0dbb\u0dbd\u0dc0-\u0dc6\u0dca\u0dcf-\u0dd4\u0dd6\u0dd8-\u0ddf\u0df2\u0df3\u0e01-\u0e3a\u0e40-\u0e4e\u0e50-\u0e59\u0e81\u0e82\u0e84\u0e87\u0e88\u0e8a\u0e8d\u0e94-\u0e97\u0e99-\u0e9f\u0ea1-\u0ea3\u0ea5\u0ea7\u0eaa\u0eab\u0ead-\u0eb9\u0ebb-\u0ebd\u0ec0-\u0ec4\u0ec6\u0ec8-\u0ecd\u0ed0-\u0ed9\u0edc-\u0edf\u0f00\u0f18\u0f19\u0f20-\u0f29\u0f35\u0f37\u0f39\u0f3e-\u0f47\u0f49-\u0f6c\u0f71-\u0f84\u0f86-\u0f97\u0f99-\u0fbc\u0fc6\u1000-\u1049\u1050-\u109d\u10a0-\u10c5\u10c7\u10cd\u10d0-\u10fa\u10fc-\u1248\u124a-\u124d\u1250-\u1256\u1258\u125a-\u125d\u1260-\u1288\u128a-\u128d\u1290-\u12b0\u12b2-\u12b5\u12b8-\u12be\u12c0\u12c2-\u12c5\u12c8-\u12d6\u12d8-\u1310\u1312-\u1315\u1318-\u135a\u135d-\u135f\u1380-\u138f\u13a0-\u13f4\u1401-\u166c\u166f-\u167f\u1681-\u169a\u16a0-\u16ea\u16ee-\u16f0\u1700-\u170c\u170e-\u1714\u1720-\u1734\u1740-\u1753\u1760-\u176c\u176e-\u1770\u1772\u1773\u1780-\u17d3\u17d7\u17dc\u17dd\u17e0-\u17e9\u180b-\u180d\u1810-\u1819\u1820-\u1877\u1880-\u18aa\u18b0-\u18f5\u1900-\u191c\u1920-\u192b\u1930-\u193b\u1946-\u196d\u1970-\u1974\u1980-\u19ab\u19b0-\u19c9\u19d0-\u19d9\u1a00-\u1a1b\u1a20-\u1a5e\u1a60-\u1a7c\u1a7f-\u1a89\u1a90-\u1a99\u1aa7\u1b00-\u1b4b\u1b50-\u1b59\u1b6b-\u1b73\u1b80-\u1bf3\u1c00-\u1c37\u1c40-\u1c49\u1c4d-\u1c7d\u1cd0-\u1cd2\u1cd4-\u1cf6\u1d00-\u1de6\u1dfc-\u1f15\u1f18-\u1f1d\u1f20-\u1f45\u1f48-\u1f4d\u1f50-\u1f57\u1f59\u1f5b\u1f5d\u1f5f-\u1f7d\u1f80-\u1fb4\u1fb6-\u1fbc\u1fbe\u1fc2-\u1fc4\u1fc6-\u1fcc\u1fd0-\u1fd3\u1fd6-\u1fdb\u1fe0-\u1fec\u1ff2-\u1ff4\u1ff6-\u1ffc\u200c\u200d\u203f\u2040\u2054\u2071\u207f\u2090-\u209c\u20d0-\u20dc\u20e1\u20e5-\u20f0\u2102\u2107\u210a-\u2113\u2115\u2119-\u211d\u2124\u2126\u2128\u212a-\u212d\u212f-\u2139\u213c-\u213f\u2145-\u2149\u214e\u2160-\u2188\u2c00-\u2c2e\u2c30-\u2c5e\u2c60-\u2ce4\u2ceb-\u2cf3\u2d00-\u2d25\u2d27\u2d2d\u2d30-\u2d67\u2d6f\u2d7f-\u2d96\u2da0-\u2da6\u2da8-\u2dae\u2db0-\u2db6\u2db8-\u2dbe\u2dc0-\u2dc6\u2dc8-\u2dce\u2dd0-\u2dd6\u2dd8-\u2dde\u2de0-\u2dff\u2e2f\u3005-\u3007\u3021-\u302f\u3031-\u3035\u3038-\u303c\u3041-\u3096\u3099\u309a\u309d-\u309f\u30a1-\u30fa\u30fc-\u30ff\u3105-\u312d\u3131-\u318e\u31a0-\u31ba\u31f0-\u31ff\u3400-\u4db5\u4e00-\u9fcc\ua000-\ua48c\ua4d0-\ua4fd\ua500-\ua60c\ua610-\ua62b\ua640-\ua66f\ua674-\ua67d\ua67f-\ua697\ua69f-\ua6f1\ua717-\ua71f\ua722-\ua788\ua78b-\ua78e\ua790-\ua793\ua7a0-\ua7aa\ua7f8-\ua827\ua840-\ua873\ua880-\ua8c4\ua8d0-\ua8d9\ua8e0-\ua8f7\ua8fb\ua900-\ua92d\ua930-\ua953\ua960-\ua97c\ua980-\ua9c0\ua9cf-\ua9d9\uaa00-\uaa36\uaa40-\uaa4d\uaa50-\uaa59\uaa60-\uaa76\uaa7a\uaa7b\uaa80-\uaac2\uaadb-\uaadd\uaae0-\uaaef\uaaf2-\uaaf6\uab01-\uab06\uab09-\uab0e\uab11-\uab16\uab20-\uab26\uab28-\uab2e\uabc0-\uabea\uabec\uabed\uabf0-\uabf9\uac00-\ud7a3\ud7b0-\ud7c6\ud7cb-\ud7fb\uf900-\ufa6d\ufa70-\ufad9\ufb00-\ufb06\ufb13-\ufb17\ufb1d-\ufb28\ufb2a-\ufb36\ufb38-\ufb3c\ufb3e\ufb40\ufb41\ufb43\ufb44\ufb46-\ufbb1\ufbd3-\ufd3d\ufd50-\ufd8f\ufd92-\ufdc7\ufdf0-\ufdfb\ufe00-\ufe0f\ufe20-\ufe26\ufe33\ufe34\ufe4d-\ufe4f\ufe70-\ufe74\ufe76-\ufefc\uff10-\uff19\uff21-\uff3a\uff3f\uff41-\uff5a\uff66-\uffbe\uffc2-\uffc7\uffca-\uffcf\uffd2-\uffd7\uffda-\uffdc]') };
      function getDefaultOptions() {
        return {
          indent: null,
          base: null,
          parse: null,
          comment: false,
          format: {
            indent: {
              style: '    ',
              base: 0,
              adjustMultilineComment: false
            },
            newline: '\n',
            space: ' ',
            json: false,
            renumber: false,
            hexadecimal: false,
            quotes: 'single',
            escapeless: false,
            compact: false,
            parentheses: true,
            semicolons: true,
            safeConcatenation: false
          },
          moz: {
            starlessGenerator: false,
            parenthesizedComprehensionBlock: false
          },
          sourceMap: null,
          sourceMapRoot: null,
          sourceMapWithCode: false,
          directive: false,
          verbatim: null
        };
      }
      function stringToArray(str) {
        var length = str.length, result = [], i;
        for (i = 0; i < length; i += 1) {
          result[i] = str.charAt(i);
        }
        return result;
      }
      function stringRepeat(str, num) {
        var result = '';
        for (num |= 0; num > 0; num >>>= 1, str += str) {
          if (num & 1) {
            result += str;
          }
        }
        return result;
      }
      isArray = Array.isArray;
      if (!isArray) {
        isArray = function isArray(array) {
          return Object.prototype.toString.call(array) === '[object Array]';
        };
      }
      function SourceNodeMock(line, column, filename, chunk) {
        var result = [];
        function flatten(input) {
          var i, iz;
          if (isArray(input)) {
            for (i = 0, iz = input.length; i < iz; ++i) {
              flatten(input[i]);
            }
          } else if (input instanceof SourceNodeMock) {
            result.push(input);
          } else if (typeof input === 'string' && input) {
            result.push(input);
          }
        }
        flatten(chunk);
        this.children = result;
      }
      SourceNodeMock.prototype.toString = function toString() {
        var res = '', i, iz, node;
        for (i = 0, iz = this.children.length; i < iz; ++i) {
          node = this.children[i];
          if (node instanceof SourceNodeMock) {
            res += node.toString();
          } else {
            res += node;
          }
        }
        return res;
      };
      SourceNodeMock.prototype.replaceRight = function replaceRight(pattern, replacement) {
        var last = this.children[this.children.length - 1];
        if (last instanceof SourceNodeMock) {
          last.replaceRight(pattern, replacement);
        } else if (typeof last === 'string') {
          this.children[this.children.length - 1] = last.replace(pattern, replacement);
        } else {
          this.children.push(''.replace(pattern, replacement));
        }
        return this;
      };
      SourceNodeMock.prototype.join = function join(sep) {
        var i, iz, result;
        result = [];
        iz = this.children.length;
        if (iz > 0) {
          for (i = 0, iz -= 1; i < iz; ++i) {
            result.push(this.children[i], sep);
          }
          result.push(this.children[iz]);
          this.children = result;
        }
        return this;
      };
      function hasLineTerminator(str) {
        return /[\r\n]/g.test(str);
      }
      function endsWithLineTerminator(str) {
        var ch = str.charAt(str.length - 1);
        return ch === '\r' || ch === '\n';
      }
      function updateDeeply(target, override) {
        var key, val;
        function isHashObject(target) {
          return typeof target === 'object' && target instanceof Object && !(target instanceof RegExp);
        }
        for (key in override) {
          if (override.hasOwnProperty(key)) {
            val = override[key];
            if (isHashObject(val)) {
              if (isHashObject(target[key])) {
                updateDeeply(target[key], val);
              } else {
                target[key] = updateDeeply({}, val);
              }
            } else {
              target[key] = val;
            }
          }
        }
        return target;
      }
      function generateNumber(value) {
        var result, point, temp, exponent, pos;
        if (value !== value) {
          throw new Error('Numeric literal whose value is NaN');
        }
        if (value < 0 || value === 0 && 1 / value < 0) {
          throw new Error('Numeric literal whose value is negative');
        }
        if (value === 1 / 0) {
          return json ? 'null' : renumber ? '1e400' : '1e+400';
        }
        result = '' + value;
        if (!renumber || result.length < 3) {
          return result;
        }
        point = result.indexOf('.');
        if (!json && result.charAt(0) === '0' && point === 1) {
          point = 0;
          result = result.slice(1);
        }
        temp = result;
        result = result.replace('e+', 'e');
        exponent = 0;
        if ((pos = temp.indexOf('e')) > 0) {
          exponent = +temp.slice(pos + 1);
          temp = temp.slice(0, pos);
        }
        if (point >= 0) {
          exponent -= temp.length - point - 1;
          temp = +(temp.slice(0, point) + temp.slice(point + 1)) + '';
        }
        pos = 0;
        while (temp.charAt(temp.length + pos - 1) === '0') {
          pos -= 1;
        }
        if (pos !== 0) {
          exponent -= pos;
          temp = temp.slice(0, pos);
        }
        if (exponent !== 0) {
          temp += 'e' + exponent;
        }
        if ((temp.length < result.length || hexadecimal && value > 1e12 && Math.floor(value) === value && (temp = '0x' + value.toString(16)).length < result.length) && +temp === value) {
          result = temp;
        }
        return result;
      }
      function escapeRegExpCharacter(ch, previousIsBackslash) {
        if ((ch & ~1) === 8232) {
          return (previousIsBackslash ? 'u' : '\\u') + (ch === 8232 ? '2028' : '2029');
        } else if (ch === 10 || ch === 13) {
          return (previousIsBackslash ? '' : '\\') + (ch === 10 ? 'n' : 'r');
        }
        return String.fromCharCode(ch);
      }
      function generateRegExp(reg) {
        var match, result, flags, i, iz, ch, characterInBrack, previousIsBackslash;
        result = reg.toString();
        if (reg.source) {
          match = result.match(/\/([^/]*)$/);
          if (!match) {
            return result;
          }
          flags = match[1];
          result = '';
          characterInBrack = false;
          previousIsBackslash = false;
          for (i = 0, iz = reg.source.length; i < iz; ++i) {
            ch = reg.source.charCodeAt(i);
            if (!previousIsBackslash) {
              if (characterInBrack) {
                if (ch === 93) {
                  characterInBrack = false;
                }
              } else {
                if (ch === 47) {
                  result += '\\';
                } else if (ch === 91) {
                  characterInBrack = true;
                }
              }
              result += escapeRegExpCharacter(ch, previousIsBackslash);
              previousIsBackslash = ch === 92;
            } else {
              result += escapeRegExpCharacter(ch, previousIsBackslash);
              previousIsBackslash = false;
            }
          }
          return '/' + result + '/' + flags;
        }
        return result;
      }
      function escapeAllowedCharacter(ch, next) {
        var code = ch.charCodeAt(0), hex = code.toString(16), result = '\\';
        switch (ch) {
        case '\b':
          result += 'b';
          break;
        case '\f':
          result += 'f';
          break;
        case '\t':
          result += 't';
          break;
        default:
          if (json || code > 255) {
            result += 'u' + '0000'.slice(hex.length) + hex;
          } else if (ch === '\0' && '0123456789'.indexOf(next) < 0) {
            result += '0';
          } else if (ch === '\x0B') {
            result += 'x0B';
          } else {
            result += 'x' + '00'.slice(hex.length) + hex;
          }
          break;
        }
        return result;
      }
      function escapeDisallowedCharacter(ch) {
        var result = '\\';
        switch (ch) {
        case '\\':
          result += '\\';
          break;
        case '\n':
          result += 'n';
          break;
        case '\r':
          result += 'r';
          break;
        case '\u2028':
          result += 'u2028';
          break;
        case '\u2029':
          result += 'u2029';
          break;
        default:
          throw new Error('Incorrectly classified character');
        }
        return result;
      }
      function escapeDirective(str) {
        var i, iz, ch, buf, quote;
        buf = str;
        if (typeof buf[0] === 'undefined') {
          buf = stringToArray(buf);
        }
        quote = quotes === 'double' ? '"' : "'";
        for (i = 0, iz = buf.length; i < iz; i += 1) {
          ch = buf[i];
          if (ch === "'") {
            quote = '"';
            break;
          } else if (ch === '"') {
            quote = "'";
            break;
          } else if (ch === '\\') {
            i += 1;
          }
        }
        return quote + str + quote;
      }
      function escapeString(str) {
        var result = '', i, len, ch, singleQuotes = 0, doubleQuotes = 0, single;
        if (typeof str[0] === 'undefined') {
          str = stringToArray(str);
        }
        for (i = 0, len = str.length; i < len; i += 1) {
          ch = str[i];
          if (ch === "'") {
            singleQuotes += 1;
          } else if (ch === '"') {
            doubleQuotes += 1;
          } else if (ch === '/' && json) {
            result += '\\';
          } else if ('\\\n\r\u2028\u2029'.indexOf(ch) >= 0) {
            result += escapeDisallowedCharacter(ch);
            continue;
          } else if (json && ch < ' ' || !(json || escapeless || ch >= ' ' && ch <= '~')) {
            result += escapeAllowedCharacter(ch, str[i + 1]);
            continue;
          }
          result += ch;
        }
        single = !(quotes === 'double' || quotes === 'auto' && doubleQuotes < singleQuotes);
        str = result;
        result = single ? "'" : '"';
        if (typeof str[0] === 'undefined') {
          str = stringToArray(str);
        }
        for (i = 0, len = str.length; i < len; i += 1) {
          ch = str[i];
          if (ch === "'" && single || ch === '"' && !single) {
            result += '\\';
          }
          result += ch;
        }
        return result + (single ? "'" : '"');
      }
      function isWhiteSpace(ch) {
        return '\t\x0B\f \xa0'.indexOf(ch) >= 0 || ch.charCodeAt(0) >= 5760 && '\u1680\u180e\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u202f\u205f\u3000\ufeff'.indexOf(ch) >= 0;
      }
      function isLineTerminator(ch) {
        return '\n\r\u2028\u2029'.indexOf(ch) >= 0;
      }
      function isIdentifierPart(ch) {
        return ch === '$' || ch === '_' || ch === '\\' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch >= '0' && ch <= '9' || ch.charCodeAt(0) >= 128 && Regex.NonAsciiIdentifierPart.test(ch);
      }
      function isDecimalDigit(ch) {
        return ch >= 48 && ch <= 57;
      }
      function toSourceNode(generated, node) {
        if (node == null) {
          if (generated instanceof SourceNode) {
            return generated;
          } else {
            node = {};
          }
        }
        if (node.loc == null) {
          return new SourceNode(null, null, sourceMap, generated, node.name || null);
        }
        return new SourceNode(node.loc.start.line, node.loc.start.column, sourceMap === true ? node.loc.source || null : sourceMap, generated, node.name || null);
      }
      function noEmptySpace() {
        return space ? space : ' ';
      }
      function join(left, right) {
        var leftSource = toSourceNode(left).toString(), rightSource = toSourceNode(right).toString(), leftChar = leftSource.charAt(leftSource.length - 1), rightChar = rightSource.charAt(0);
        if ((leftChar === '+' || leftChar === '-') && leftChar === rightChar || isIdentifierPart(leftChar) && isIdentifierPart(rightChar) || leftChar === '/' && rightChar === 'i') {
          return [
            left,
            noEmptySpace(),
            right
          ];
        } else if (isWhiteSpace(leftChar) || isLineTerminator(leftChar) || isWhiteSpace(rightChar) || isLineTerminator(rightChar)) {
          return [
            left,
            right
          ];
        }
        return [
          left,
          space,
          right
        ];
      }
      function addIndent(stmt) {
        return [
          base,
          stmt
        ];
      }
      function withIndent(fn) {
        var previousBase, result;
        previousBase = base;
        base += indent;
        result = fn.call(this, base);
        base = previousBase;
        return result;
      }
      function calculateSpaces(str) {
        var i;
        for (i = str.length - 1; i >= 0; i -= 1) {
          if (isLineTerminator(str.charAt(i))) {
            break;
          }
        }
        return str.length - 1 - i;
      }
      function adjustMultilineComment(value, specialBase) {
        var array, i, len, line, j, spaces, previousBase;
        array = value.split(/\r\n|[\r\n]/);
        spaces = Number.MAX_VALUE;
        for (i = 1, len = array.length; i < len; i += 1) {
          line = array[i];
          j = 0;
          while (j < line.length && isWhiteSpace(line[j])) {
            j += 1;
          }
          if (spaces > j) {
            spaces = j;
          }
        }
        if (typeof specialBase !== 'undefined') {
          previousBase = base;
          if (array[1][spaces] === '*') {
            specialBase += ' ';
          }
          base = specialBase;
        } else {
          if (spaces & 1) {
            spaces -= 1;
          }
          previousBase = base;
        }
        for (i = 1, len = array.length; i < len; i += 1) {
          array[i] = toSourceNode(addIndent(array[i].slice(spaces))).join('');
        }
        base = previousBase;
        return array.join('\n');
      }
      function generateComment(comment, specialBase) {
        if (comment.type === 'Line') {
          if (endsWithLineTerminator(comment.value)) {
            return '//' + comment.value;
          } else {
            return '//' + comment.value + '\n';
          }
        }
        if (extra.format.indent.adjustMultilineComment && /[\n\r]/.test(comment.value)) {
          return adjustMultilineComment('/*' + comment.value + '*/', specialBase);
        }
        return '/*' + comment.value + '*/';
      }
      function addCommentsToStatement(stmt, result) {
        var i, len, comment, save, tailingToStatement, specialBase, fragment;
        if (stmt.leadingComments && stmt.leadingComments.length > 0) {
          save = result;
          comment = stmt.leadingComments[0];
          result = [];
          if (safeConcatenation && stmt.type === Syntax.Program && stmt.body.length === 0) {
            result.push('\n');
          }
          result.push(generateComment(comment));
          if (!endsWithLineTerminator(toSourceNode(result).toString())) {
            result.push('\n');
          }
          for (i = 1, len = stmt.leadingComments.length; i < len; i += 1) {
            comment = stmt.leadingComments[i];
            fragment = [generateComment(comment)];
            if (!endsWithLineTerminator(toSourceNode(fragment).toString())) {
              fragment.push('\n');
            }
            result.push(addIndent(fragment));
          }
          result.push(addIndent(save));
        }
        if (stmt.trailingComments) {
          tailingToStatement = !endsWithLineTerminator(toSourceNode(result).toString());
          specialBase = stringRepeat(' ', calculateSpaces(toSourceNode([
            base,
            result,
            indent
          ]).toString()));
          for (i = 0, len = stmt.trailingComments.length; i < len; i += 1) {
            comment = stmt.trailingComments[i];
            if (tailingToStatement) {
              if (i === 0) {
                result = [
                  result,
                  indent
                ];
              } else {
                result = [
                  result,
                  specialBase
                ];
              }
              result.push(generateComment(comment, specialBase));
            } else {
              result = [
                result,
                addIndent(generateComment(comment))
              ];
            }
            if (i !== len - 1 && !endsWithLineTerminator(toSourceNode(result).toString())) {
              result = [
                result,
                '\n'
              ];
            }
          }
        }
        return result;
      }
      function parenthesize(text, current, should) {
        if (current < should) {
          return [
            '(',
            text,
            ')'
          ];
        }
        return text;
      }
      function maybeBlock(stmt, semicolonOptional, functionBody) {
        var result, noLeadingComment;
        noLeadingComment = !extra.comment || !stmt.leadingComments;
        if (stmt.type === Syntax.BlockStatement && noLeadingComment) {
          return [
            space,
            generateStatement(stmt, { functionBody: functionBody })
          ];
        }
        if (stmt.type === Syntax.EmptyStatement && noLeadingComment) {
          return ';';
        }
        withIndent(function () {
          result = [
            newline,
            addIndent(generateStatement(stmt, {
              semicolonOptional: semicolonOptional,
              functionBody: functionBody
            }))
          ];
        });
        return result;
      }
      function maybeBlockSuffix(stmt, result) {
        var ends = endsWithLineTerminator(toSourceNode(result).toString());
        if (stmt.type === Syntax.BlockStatement && (!extra.comment || !stmt.leadingComments) && !ends) {
          return [
            result,
            space
          ];
        }
        if (ends) {
          return [
            result,
            base
          ];
        }
        return [
          result,
          newline,
          base
        ];
      }
      function generateVerbatim(expr, option) {
        var i, result;
        result = expr[extra.verbatim].split(/\r\n|\n/);
        for (i = 1; i < result.length; i++) {
          result[i] = newline + base + result[i];
        }
        result = parenthesize(result, Precedence.Sequence, option.precedence);
        return toSourceNode(result, expr);
      }
      function generateIdentifier(node) {
        return toSourceNode(node.name, node);
      }
      function generateFunctionBody(node) {
        var result, i, len, expr, arrow;
        arrow = node.type === Syntax.ArrowFunctionExpression;
        if (arrow && node.params.length === 1 && node.params[0].type === Syntax.Identifier) {
          result = [generateIdentifier(node.params[0])];
        } else {
          result = ['('];
          for (i = 0, len = node.params.length; i < len; i += 1) {
            result.push(generateIdentifier(node.params[i]));
            if (i + 1 < len) {
              result.push(',' + space);
            }
          }
          result.push(')');
        }
        if (arrow) {
          result.push(space, '=>');
        }
        if (node.expression) {
          result.push(space);
          expr = generateExpression(node.body, {
            precedence: Precedence.Assignment,
            allowIn: true,
            allowCall: true
          });
          if (expr.toString().charAt(0) === '{') {
            expr = [
              '(',
              expr,
              ')'
            ];
          }
          result.push(expr);
        } else {
          result.push(maybeBlock(node.body, false, true));
        }
        return result;
      }
      function generateExpression(expr, option) {
        var result, precedence, type, currentPrecedence, i, len, raw, fragment, multiline, leftChar, leftSource, rightChar, allowIn, allowCall, allowUnparenthesizedNew, property;
        precedence = option.precedence;
        allowIn = option.allowIn;
        allowCall = option.allowCall;
        type = expr.type || option.type;
        if (extra.verbatim && expr.hasOwnProperty(extra.verbatim)) {
          return generateVerbatim(expr, option);
        }
        switch (type) {
        case Syntax.SequenceExpression:
          result = [];
          allowIn |= Precedence.Sequence < precedence;
          for (i = 0, len = expr.expressions.length; i < len; i += 1) {
            result.push(generateExpression(expr.expressions[i], {
              precedence: Precedence.Assignment,
              allowIn: allowIn,
              allowCall: true
            }));
            if (i + 1 < len) {
              result.push(',' + space);
            }
          }
          result = parenthesize(result, Precedence.Sequence, precedence);
          break;
        case Syntax.AssignmentExpression:
          allowIn |= Precedence.Assignment < precedence;
          result = parenthesize([
            generateExpression(expr.left, {
              precedence: Precedence.Call,
              allowIn: allowIn,
              allowCall: true
            }),
            space + expr.operator + space,
            generateExpression(expr.right, {
              precedence: Precedence.Assignment,
              allowIn: allowIn,
              allowCall: true
            })
          ], Precedence.Assignment, precedence);
          break;
        case Syntax.ArrowFunctionExpression:
          allowIn |= Precedence.ArrowFunction < precedence;
          result = parenthesize(generateFunctionBody(expr), Precedence.ArrowFunction, precedence);
          break;
        case Syntax.ConditionalExpression:
          allowIn |= Precedence.Conditional < precedence;
          result = parenthesize([
            generateExpression(expr.test, {
              precedence: Precedence.LogicalOR,
              allowIn: allowIn,
              allowCall: true
            }),
            space + '?' + space,
            generateExpression(expr.consequent, {
              precedence: Precedence.Assignment,
              allowIn: allowIn,
              allowCall: true
            }),
            space + ':' + space,
            generateExpression(expr.alternate, {
              precedence: Precedence.Assignment,
              allowIn: allowIn,
              allowCall: true
            })
          ], Precedence.Conditional, precedence);
          break;
        case Syntax.LogicalExpression:
        case Syntax.BinaryExpression:
          currentPrecedence = BinaryPrecedence[expr.operator];
          allowIn |= currentPrecedence < precedence;
          fragment = generateExpression(expr.left, {
            precedence: currentPrecedence,
            allowIn: allowIn,
            allowCall: true
          });
          leftSource = fragment.toString();
          if (leftSource.charAt(leftSource.length - 1) === '/' && isIdentifierPart(expr.operator.charAt(0))) {
            result = [
              fragment,
              noEmptySpace(),
              expr.operator
            ];
          } else {
            result = join(fragment, expr.operator);
          }
          fragment = generateExpression(expr.right, {
            precedence: currentPrecedence + 1,
            allowIn: allowIn,
            allowCall: true
          });
          if (expr.operator === '/' && fragment.toString().charAt(0) === '/' || expr.operator.slice(-1) === '<' && fragment.toString().slice(0, 3) === '!--') {
            result.push(noEmptySpace(), fragment);
          } else {
            result = join(result, fragment);
          }
          if (expr.operator === 'in' && !allowIn) {
            result = [
              '(',
              result,
              ')'
            ];
          } else {
            result = parenthesize(result, currentPrecedence, precedence);
          }
          break;
        case Syntax.CallExpression:
          result = [generateExpression(expr.callee, {
              precedence: Precedence.Call,
              allowIn: true,
              allowCall: true,
              allowUnparenthesizedNew: false
            })];
          result.push('(');
          for (i = 0, len = expr['arguments'].length; i < len; i += 1) {
            result.push(generateExpression(expr['arguments'][i], {
              precedence: Precedence.Assignment,
              allowIn: true,
              allowCall: true
            }));
            if (i + 1 < len) {
              result.push(',' + space);
            }
          }
          result.push(')');
          if (!allowCall) {
            result = [
              '(',
              result,
              ')'
            ];
          } else {
            result = parenthesize(result, Precedence.Call, precedence);
          }
          break;
        case Syntax.NewExpression:
          len = expr['arguments'].length;
          allowUnparenthesizedNew = option.allowUnparenthesizedNew === undefined || option.allowUnparenthesizedNew;
          result = join('new', generateExpression(expr.callee, {
            precedence: Precedence.New,
            allowIn: true,
            allowCall: false,
            allowUnparenthesizedNew: allowUnparenthesizedNew && !parentheses && len === 0
          }));
          if (!allowUnparenthesizedNew || parentheses || len > 0) {
            result.push('(');
            for (i = 0; i < len; i += 1) {
              result.push(generateExpression(expr['arguments'][i], {
                precedence: Precedence.Assignment,
                allowIn: true,
                allowCall: true
              }));
              if (i + 1 < len) {
                result.push(',' + space);
              }
            }
            result.push(')');
          }
          result = parenthesize(result, Precedence.New, precedence);
          break;
        case Syntax.MemberExpression:
          result = [generateExpression(expr.object, {
              precedence: Precedence.Call,
              allowIn: true,
              allowCall: allowCall,
              allowUnparenthesizedNew: false
            })];
          if (expr.computed) {
            result.push('[', generateExpression(expr.property, {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: allowCall
            }), ']');
          } else {
            if (expr.object.type === Syntax.Literal && typeof expr.object.value === 'number') {
              fragment = toSourceNode(result).toString();
              if (fragment.indexOf('.') < 0 && !/[eExX]/.test(fragment) && isDecimalDigit(fragment.charCodeAt(fragment.length - 1)) && !(fragment.length >= 2 && fragment.charCodeAt(0) === 48)) {
                result.push('.');
              }
            }
            result.push('.', generateIdentifier(expr.property));
          }
          result = parenthesize(result, Precedence.Member, precedence);
          break;
        case Syntax.UnaryExpression:
          fragment = generateExpression(expr.argument, {
            precedence: Precedence.Unary,
            allowIn: true,
            allowCall: true
          });
          if (space === '') {
            result = join(expr.operator, fragment);
          } else {
            result = [expr.operator];
            if (expr.operator.length > 2) {
              result = join(result, fragment);
            } else {
              leftSource = toSourceNode(result).toString();
              leftChar = leftSource.charAt(leftSource.length - 1);
              rightChar = fragment.toString().charAt(0);
              if ((leftChar === '+' || leftChar === '-') && leftChar === rightChar || isIdentifierPart(leftChar) && isIdentifierPart(rightChar)) {
                result.push(noEmptySpace(), fragment);
              } else {
                result.push(fragment);
              }
            }
          }
          result = parenthesize(result, Precedence.Unary, precedence);
          break;
        case Syntax.YieldExpression:
          if (expr.delegate) {
            result = 'yield*';
          } else {
            result = 'yield';
          }
          if (expr.argument) {
            result = join(result, generateExpression(expr.argument, {
              precedence: Precedence.Assignment,
              allowIn: true,
              allowCall: true
            }));
          }
          break;
        case Syntax.UpdateExpression:
          if (expr.prefix) {
            result = parenthesize([
              expr.operator,
              generateExpression(expr.argument, {
                precedence: Precedence.Unary,
                allowIn: true,
                allowCall: true
              })
            ], Precedence.Unary, precedence);
          } else {
            result = parenthesize([
              generateExpression(expr.argument, {
                precedence: Precedence.Postfix,
                allowIn: true,
                allowCall: true
              }),
              expr.operator
            ], Precedence.Postfix, precedence);
          }
          break;
        case Syntax.FunctionExpression:
          result = 'function';
          if (expr.id) {
            result = [
              result,
              noEmptySpace(),
              generateIdentifier(expr.id),
              generateFunctionBody(expr)
            ];
          } else {
            result = [
              result + space,
              generateFunctionBody(expr)
            ];
          }
          break;
        case Syntax.ArrayPattern:
        case Syntax.ArrayExpression:
          if (!expr.elements.length) {
            result = '[]';
            break;
          }
          multiline = expr.elements.length > 1;
          result = [
            '[',
            multiline ? newline : ''
          ];
          withIndent(function (indent) {
            for (i = 0, len = expr.elements.length; i < len; i += 1) {
              if (!expr.elements[i]) {
                if (multiline) {
                  result.push(indent);
                }
                if (i + 1 === len) {
                  result.push(',');
                }
              } else {
                result.push(multiline ? indent : '', generateExpression(expr.elements[i], {
                  precedence: Precedence.Assignment,
                  allowIn: true,
                  allowCall: true
                }));
              }
              if (i + 1 < len) {
                result.push(',' + (multiline ? newline : space));
              }
            }
          });
          if (multiline && !endsWithLineTerminator(toSourceNode(result).toString())) {
            result.push(newline);
          }
          result.push(multiline ? base : '', ']');
          break;
        case Syntax.Property:
          if (expr.kind === 'get' || expr.kind === 'set') {
            result = [
              expr.kind,
              noEmptySpace(),
              generateExpression(expr.key, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              generateFunctionBody(expr.value)
            ];
          } else {
            if (expr.shorthand) {
              result = generateExpression(expr.key, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              });
            } else if (expr.method) {
              result = [];
              if (expr.value.generator) {
                result.push('*');
              }
              result.push(generateExpression(expr.key, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }), generateFunctionBody(expr.value));
            } else {
              result = [
                generateExpression(expr.key, {
                  precedence: Precedence.Sequence,
                  allowIn: true,
                  allowCall: true
                }),
                ':' + space,
                generateExpression(expr.value, {
                  precedence: Precedence.Assignment,
                  allowIn: true,
                  allowCall: true
                })
              ];
            }
          }
          break;
        case Syntax.ObjectExpression:
          if (!expr.properties.length) {
            result = '{}';
            break;
          }
          multiline = expr.properties.length > 1;
          withIndent(function () {
            fragment = generateExpression(expr.properties[0], {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: true,
              type: Syntax.Property
            });
          });
          if (!multiline) {
            if (!hasLineTerminator(toSourceNode(fragment).toString())) {
              result = [
                '{',
                space,
                fragment,
                space,
                '}'
              ];
              break;
            }
          }
          withIndent(function (indent) {
            result = [
              '{',
              newline,
              indent,
              fragment
            ];
            if (multiline) {
              result.push(',' + newline);
              for (i = 1, len = expr.properties.length; i < len; i += 1) {
                result.push(indent, generateExpression(expr.properties[i], {
                  precedence: Precedence.Sequence,
                  allowIn: true,
                  allowCall: true,
                  type: Syntax.Property
                }));
                if (i + 1 < len) {
                  result.push(',' + newline);
                }
              }
            }
          });
          if (!endsWithLineTerminator(toSourceNode(result).toString())) {
            result.push(newline);
          }
          result.push(base, '}');
          break;
        case Syntax.ObjectPattern:
          if (!expr.properties.length) {
            result = '{}';
            break;
          }
          multiline = false;
          if (expr.properties.length === 1) {
            property = expr.properties[0];
            if (property.value.type !== Syntax.Identifier) {
              multiline = true;
            }
          } else {
            for (i = 0, len = expr.properties.length; i < len; i += 1) {
              property = expr.properties[i];
              if (!property.shorthand) {
                multiline = true;
                break;
              }
            }
          }
          result = [
            '{',
            multiline ? newline : ''
          ];
          withIndent(function (indent) {
            for (i = 0, len = expr.properties.length; i < len; i += 1) {
              result.push(multiline ? indent : '', generateExpression(expr.properties[i], {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }));
              if (i + 1 < len) {
                result.push(',' + (multiline ? newline : space));
              }
            }
          });
          if (multiline && !endsWithLineTerminator(toSourceNode(result).toString())) {
            result.push(newline);
          }
          result.push(multiline ? base : '', '}');
          break;
        case Syntax.ThisExpression:
          result = 'this';
          break;
        case Syntax.Identifier:
          result = generateIdentifier(expr);
          break;
        case Syntax.Literal:
          if (expr.hasOwnProperty('raw') && parse) {
            try {
              raw = parse(expr.raw).body[0].expression;
              if (raw.type === Syntax.Literal) {
                if (raw.value === expr.value) {
                  result = expr.raw;
                  break;
                }
              }
            } catch (e) {
            }
          }
          if (expr.value === null) {
            result = 'null';
            break;
          }
          if (typeof expr.value === 'string') {
            result = escapeString(expr.value);
            break;
          }
          if (typeof expr.value === 'number') {
            result = generateNumber(expr.value);
            break;
          }
          if (typeof expr.value === 'boolean') {
            result = expr.value ? 'true' : 'false';
            break;
          }
          result = generateRegExp(expr.value);
          break;
        case Syntax.ComprehensionExpression:
          result = [
            '[',
            generateExpression(expr.body, {
              precedence: Precedence.Assignment,
              allowIn: true,
              allowCall: true
            })
          ];
          if (expr.blocks) {
            for (i = 0, len = expr.blocks.length; i < len; i += 1) {
              fragment = generateExpression(expr.blocks[i], {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              });
              result = join(result, fragment);
            }
          }
          if (expr.filter) {
            result = join(result, 'if' + space);
            fragment = generateExpression(expr.filter, {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: true
            });
            if (extra.moz.parenthesizedComprehensionBlock) {
              result = join(result, [
                '(',
                fragment,
                ')'
              ]);
            } else {
              result = join(result, fragment);
            }
          }
          result.push(']');
          break;
        case Syntax.ComprehensionBlock:
          if (expr.left.type === Syntax.VariableDeclaration) {
            fragment = [
              expr.left.kind,
              noEmptySpace(),
              generateStatement(expr.left.declarations[0], { allowIn: false })
            ];
          } else {
            fragment = generateExpression(expr.left, {
              precedence: Precedence.Call,
              allowIn: true,
              allowCall: true
            });
          }
          fragment = join(fragment, expr.of ? 'of' : 'in');
          fragment = join(fragment, generateExpression(expr.right, {
            precedence: Precedence.Sequence,
            allowIn: true,
            allowCall: true
          }));
          if (extra.moz.parenthesizedComprehensionBlock) {
            result = [
              'for' + space + '(',
              fragment,
              ')'
            ];
          } else {
            result = join('for' + space, fragment);
          }
          break;
        default:
          throw new Error('Unknown expression type: ' + expr.type);
        }
        return toSourceNode(result, expr);
      }
      function generateStatement(stmt, option) {
        var i, len, result, node, allowIn, functionBody, directiveContext, fragment, semicolon;
        allowIn = true;
        semicolon = ';';
        functionBody = false;
        directiveContext = false;
        if (option) {
          allowIn = option.allowIn === undefined || option.allowIn;
          if (!semicolons && option.semicolonOptional === true) {
            semicolon = '';
          }
          functionBody = option.functionBody;
          directiveContext = option.directiveContext;
        }
        switch (stmt.type) {
        case Syntax.BlockStatement:
          result = [
            '{',
            newline
          ];
          withIndent(function () {
            for (i = 0, len = stmt.body.length; i < len; i += 1) {
              fragment = addIndent(generateStatement(stmt.body[i], {
                semicolonOptional: i === len - 1,
                directiveContext: functionBody
              }));
              result.push(fragment);
              if (!endsWithLineTerminator(toSourceNode(fragment).toString())) {
                result.push(newline);
              }
            }
          });
          result.push(addIndent('}'));
          break;
        case Syntax.BreakStatement:
          if (stmt.label) {
            result = 'break ' + stmt.label.name + semicolon;
          } else {
            result = 'break' + semicolon;
          }
          break;
        case Syntax.ContinueStatement:
          if (stmt.label) {
            result = 'continue ' + stmt.label.name + semicolon;
          } else {
            result = 'continue' + semicolon;
          }
          break;
        case Syntax.DirectiveStatement:
          if (stmt.raw) {
            result = stmt.raw + semicolon;
          } else {
            result = escapeDirective(stmt.directive) + semicolon;
          }
          break;
        case Syntax.DoWhileStatement:
          result = join('do', maybeBlock(stmt.body));
          result = maybeBlockSuffix(stmt.body, result);
          result = join(result, [
            'while' + space + '(',
            generateExpression(stmt.test, {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: true
            }),
            ')' + semicolon
          ]);
          break;
        case Syntax.CatchClause:
          withIndent(function () {
            result = [
              'catch' + space + '(',
              generateExpression(stmt.param, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              ')'
            ];
          });
          result.push(maybeBlock(stmt.body));
          break;
        case Syntax.DebuggerStatement:
          result = 'debugger' + semicolon;
          break;
        case Syntax.EmptyStatement:
          result = ';';
          break;
        case Syntax.ExpressionStatement:
          result = [generateExpression(stmt.expression, {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: true
            })];
          fragment = toSourceNode(result).toString();
          if (fragment.charAt(0) === '{' || fragment.slice(0, 8) === 'function' && ' ('.indexOf(fragment.charAt(8)) >= 0 || directive && directiveContext && stmt.expression.type === Syntax.Literal && typeof stmt.expression.value === 'string') {
            result = [
              '(',
              result,
              ')' + semicolon
            ];
          } else {
            result.push(semicolon);
          }
          break;
        case Syntax.VariableDeclarator:
          if (stmt.init) {
            result = [
              generateExpression(stmt.id, {
                precedence: Precedence.Assignment,
                allowIn: allowIn,
                allowCall: true
              }),
              space,
              '=',
              space,
              generateExpression(stmt.init, {
                precedence: Precedence.Assignment,
                allowIn: allowIn,
                allowCall: true
              })
            ];
          } else {
            result = generateIdentifier(stmt.id);
          }
          break;
        case Syntax.VariableDeclaration:
          result = [stmt.kind];
          if (stmt.declarations.length === 1 && stmt.declarations[0].init && stmt.declarations[0].init.type === Syntax.FunctionExpression) {
            result.push(noEmptySpace(), generateStatement(stmt.declarations[0], { allowIn: allowIn }));
          } else {
            withIndent(function () {
              node = stmt.declarations[0];
              if (extra.comment && node.leadingComments) {
                result.push('\n', addIndent(generateStatement(node, { allowIn: allowIn })));
              } else {
                result.push(noEmptySpace(), generateStatement(node, { allowIn: allowIn }));
              }
              for (i = 1, len = stmt.declarations.length; i < len; i += 1) {
                node = stmt.declarations[i];
                if (extra.comment && node.leadingComments) {
                  result.push(',' + newline, addIndent(generateStatement(node, { allowIn: allowIn })));
                } else {
                  result.push(',' + space, generateStatement(node, { allowIn: allowIn }));
                }
              }
            });
          }
          result.push(semicolon);
          break;
        case Syntax.ThrowStatement:
          result = [
            join('throw', generateExpression(stmt.argument, {
              precedence: Precedence.Sequence,
              allowIn: true,
              allowCall: true
            })),
            semicolon
          ];
          break;
        case Syntax.TryStatement:
          result = [
            'try',
            maybeBlock(stmt.block)
          ];
          result = maybeBlockSuffix(stmt.block, result);
          if (stmt.handlers) {
            for (i = 0, len = stmt.handlers.length; i < len; i += 1) {
              result = join(result, generateStatement(stmt.handlers[i]));
              if (stmt.finalizer || i + 1 !== len) {
                result = maybeBlockSuffix(stmt.handlers[i].body, result);
              }
            }
          } else {
            if (stmt.handler) {
              result = join(result, generateStatement(stmt.handler));
              if (stmt.finalizer || stmt.guardedHandlers.length > 0) {
                result = maybeBlockSuffix(stmt.handler.body, result);
              }
            }
            for (i = 0, len = stmt.guardedHandlers.length; i < len; i += 1) {
              result = join(result, generateStatement(stmt.guardedHandlers[i]));
              if (stmt.finalizer || i + 1 !== len) {
                result = maybeBlockSuffix(stmt.guardedHandlers[i].body, result);
              }
            }
          }
          if (stmt.finalizer) {
            result = join(result, [
              'finally',
              maybeBlock(stmt.finalizer)
            ]);
          }
          break;
        case Syntax.SwitchStatement:
          withIndent(function () {
            result = [
              'switch' + space + '(',
              generateExpression(stmt.discriminant, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              ')' + space + '{' + newline
            ];
          });
          if (stmt.cases) {
            for (i = 0, len = stmt.cases.length; i < len; i += 1) {
              fragment = addIndent(generateStatement(stmt.cases[i], { semicolonOptional: i === len - 1 }));
              result.push(fragment);
              if (!endsWithLineTerminator(toSourceNode(fragment).toString())) {
                result.push(newline);
              }
            }
          }
          result.push(addIndent('}'));
          break;
        case Syntax.SwitchCase:
          withIndent(function () {
            if (stmt.test) {
              result = [
                join('case', generateExpression(stmt.test, {
                  precedence: Precedence.Sequence,
                  allowIn: true,
                  allowCall: true
                })),
                ':'
              ];
            } else {
              result = ['default:'];
            }
            i = 0;
            len = stmt.consequent.length;
            if (len && stmt.consequent[0].type === Syntax.BlockStatement) {
              fragment = maybeBlock(stmt.consequent[0]);
              result.push(fragment);
              i = 1;
            }
            if (i !== len && !endsWithLineTerminator(toSourceNode(result).toString())) {
              result.push(newline);
            }
            for (; i < len; i += 1) {
              fragment = addIndent(generateStatement(stmt.consequent[i], { semicolonOptional: i === len - 1 && semicolon === '' }));
              result.push(fragment);
              if (i + 1 !== len && !endsWithLineTerminator(toSourceNode(fragment).toString())) {
                result.push(newline);
              }
            }
          });
          break;
        case Syntax.IfStatement:
          withIndent(function () {
            result = [
              'if' + space + '(',
              generateExpression(stmt.test, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              ')'
            ];
          });
          if (stmt.alternate) {
            result.push(maybeBlock(stmt.consequent));
            result = maybeBlockSuffix(stmt.consequent, result);
            if (stmt.alternate.type === Syntax.IfStatement) {
              result = join(result, [
                'else ',
                generateStatement(stmt.alternate, { semicolonOptional: semicolon === '' })
              ]);
            } else {
              result = join(result, join('else', maybeBlock(stmt.alternate, semicolon === '')));
            }
          } else {
            result.push(maybeBlock(stmt.consequent, semicolon === ''));
          }
          break;
        case Syntax.ForStatement:
          withIndent(function () {
            result = ['for' + space + '('];
            if (stmt.init) {
              if (stmt.init.type === Syntax.VariableDeclaration) {
                result.push(generateStatement(stmt.init, { allowIn: false }));
              } else {
                result.push(generateExpression(stmt.init, {
                  precedence: Precedence.Sequence,
                  allowIn: false,
                  allowCall: true
                }), ';');
              }
            } else {
              result.push(';');
            }
            if (stmt.test) {
              result.push(space, generateExpression(stmt.test, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }), ';');
            } else {
              result.push(';');
            }
            if (stmt.update) {
              result.push(space, generateExpression(stmt.update, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }), ')');
            } else {
              result.push(')');
            }
          });
          result.push(maybeBlock(stmt.body, semicolon === ''));
          break;
        case Syntax.ForInStatement:
          result = ['for' + space + '('];
          withIndent(function () {
            if (stmt.left.type === Syntax.VariableDeclaration) {
              withIndent(function () {
                result.push(stmt.left.kind + noEmptySpace(), generateStatement(stmt.left.declarations[0], { allowIn: false }));
              });
            } else {
              result.push(generateExpression(stmt.left, {
                precedence: Precedence.Call,
                allowIn: true,
                allowCall: true
              }));
            }
            result = join(result, 'in');
            result = [
              join(result, generateExpression(stmt.right, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              })),
              ')'
            ];
          });
          result.push(maybeBlock(stmt.body, semicolon === ''));
          break;
        case Syntax.LabeledStatement:
          result = [
            stmt.label.name + ':',
            maybeBlock(stmt.body, semicolon === '')
          ];
          break;
        case Syntax.Program:
          len = stmt.body.length;
          result = [safeConcatenation && len > 0 ? '\n' : ''];
          for (i = 0; i < len; i += 1) {
            fragment = addIndent(generateStatement(stmt.body[i], {
              semicolonOptional: !safeConcatenation && i === len - 1,
              directiveContext: true
            }));
            result.push(fragment);
            if (i + 1 < len && !endsWithLineTerminator(toSourceNode(fragment).toString())) {
              result.push(newline);
            }
          }
          break;
        case Syntax.FunctionDeclaration:
          result = [
            stmt.generator && !extra.moz.starlessGenerator ? 'function* ' : 'function ',
            generateIdentifier(stmt.id),
            generateFunctionBody(stmt)
          ];
          break;
        case Syntax.ReturnStatement:
          if (stmt.argument) {
            result = [
              join('return', generateExpression(stmt.argument, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              })),
              semicolon
            ];
          } else {
            result = ['return' + semicolon];
          }
          break;
        case Syntax.WhileStatement:
          withIndent(function () {
            result = [
              'while' + space + '(',
              generateExpression(stmt.test, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              ')'
            ];
          });
          result.push(maybeBlock(stmt.body, semicolon === ''));
          break;
        case Syntax.WithStatement:
          withIndent(function () {
            result = [
              'with' + space + '(',
              generateExpression(stmt.object, {
                precedence: Precedence.Sequence,
                allowIn: true,
                allowCall: true
              }),
              ')'
            ];
          });
          result.push(maybeBlock(stmt.body, semicolon === ''));
          break;
        default:
          throw new Error('Unknown statement type: ' + stmt.type);
        }
        if (extra.comment) {
          result = addCommentsToStatement(stmt, result);
        }
        fragment = toSourceNode(result).toString();
        if (stmt.type === Syntax.Program && !safeConcatenation && newline === '' && fragment.charAt(fragment.length - 1) === '\n') {
          result = toSourceNode(result).replaceRight(/\s+$/, '');
        }
        return toSourceNode(result, stmt);
      }
      function generate(node, options) {
        var defaultOptions = getDefaultOptions(), result, pair;
        if (options != null) {
          if (typeof options.indent === 'string') {
            defaultOptions.format.indent.style = options.indent;
          }
          if (typeof options.base === 'number') {
            defaultOptions.format.indent.base = options.base;
          }
          options = updateDeeply(defaultOptions, options);
          indent = options.format.indent.style;
          if (typeof options.base === 'string') {
            base = options.base;
          } else {
            base = stringRepeat(indent, options.format.indent.base);
          }
        } else {
          options = defaultOptions;
          indent = options.format.indent.style;
          base = stringRepeat(indent, options.format.indent.base);
        }
        json = options.format.json;
        renumber = options.format.renumber;
        hexadecimal = json ? false : options.format.hexadecimal;
        quotes = json ? 'double' : options.format.quotes;
        escapeless = options.format.escapeless;
        newline = options.format.newline;
        space = options.format.space;
        if (options.format.compact) {
          newline = space = indent = base = '';
        }
        parentheses = options.format.parentheses;
        semicolons = options.format.semicolons;
        safeConcatenation = options.format.safeConcatenation;
        directive = options.directive;
        parse = json ? null : options.parse;
        sourceMap = options.sourceMap;
        extra = options;
        if (sourceMap) {
          if (!exports.browser) {
            SourceNode = require('/node_modules/source-map/lib/source-map.js', module).SourceNode;
          } else {
            SourceNode = global.sourceMap.SourceNode;
          }
        } else {
          SourceNode = SourceNodeMock;
        }
        switch (node.type) {
        case Syntax.BlockStatement:
        case Syntax.BreakStatement:
        case Syntax.CatchClause:
        case Syntax.ContinueStatement:
        case Syntax.DirectiveStatement:
        case Syntax.DoWhileStatement:
        case Syntax.DebuggerStatement:
        case Syntax.EmptyStatement:
        case Syntax.ExpressionStatement:
        case Syntax.ForStatement:
        case Syntax.ForInStatement:
        case Syntax.FunctionDeclaration:
        case Syntax.IfStatement:
        case Syntax.LabeledStatement:
        case Syntax.Program:
        case Syntax.ReturnStatement:
        case Syntax.SwitchStatement:
        case Syntax.SwitchCase:
        case Syntax.ThrowStatement:
        case Syntax.TryStatement:
        case Syntax.VariableDeclaration:
        case Syntax.VariableDeclarator:
        case Syntax.WhileStatement:
        case Syntax.WithStatement:
          result = generateStatement(node);
          break;
        case Syntax.AssignmentExpression:
        case Syntax.ArrayExpression:
        case Syntax.ArrayPattern:
        case Syntax.BinaryExpression:
        case Syntax.CallExpression:
        case Syntax.ConditionalExpression:
        case Syntax.FunctionExpression:
        case Syntax.Identifier:
        case Syntax.Literal:
        case Syntax.LogicalExpression:
        case Syntax.MemberExpression:
        case Syntax.NewExpression:
        case Syntax.ObjectExpression:
        case Syntax.ObjectPattern:
        case Syntax.Property:
        case Syntax.SequenceExpression:
        case Syntax.ThisExpression:
        case Syntax.UnaryExpression:
        case Syntax.UpdateExpression:
        case Syntax.YieldExpression:
          result = generateExpression(node, {
            precedence: Precedence.Sequence,
            allowIn: true,
            allowCall: true
          });
          break;
        default:
          throw new Error('Unknown node type: ' + node.type);
        }
        if (!sourceMap) {
          return result.toString();
        }
        pair = result.toStringWithSourceMap({
          file: options.file,
          sourceRoot: options.sourceMapRoot
        });
        if (options.sourceMapWithCode) {
          return pair;
        }
        return pair.map.toString();
      }
      FORMAT_MINIFY = {
        indent: {
          style: '',
          base: 0
        },
        renumber: true,
        hexadecimal: true,
        quotes: 'auto',
        escapeless: true,
        compact: true,
        parentheses: false,
        semicolons: false
      };
      FORMAT_DEFAULTS = getDefaultOptions().format;
      exports.version = require('/package.json', module).version;
      exports.generate = generate;
      exports.attachComments = estraverse.attachComments;
      exports.browser = false;
      exports.FORMAT_MINIFY = FORMAT_MINIFY;
      exports.FORMAT_DEFAULTS = FORMAT_DEFAULTS;
    }());
  });
  require.define('/package.json', function (module, exports, __dirname, __filename) {
    module.exports = {
      'name': 'escodegen',
      'description': 'ECMAScript code generator',
      'homepage': 'http://github.com/Constellation/escodegen.html',
      'main': 'escodegen.js',
      'bin': {
        'esgenerate': './bin/esgenerate.js',
        'escodegen': './bin/escodegen.js'
      },
      'version': '0.0.28-dev',
      'engines': { 'node': '>=0.4.0' },
      'maintainers': [{
          'name': 'Yusuke Suzuki',
          'email': 'utatane.tea@gmail.com',
          'web': 'http://github.com/Constellation'
        }],
      'repository': {
        'type': 'git',
        'url': 'http://github.com/Constellation/escodegen.git'
      },
      'dependencies': {
        'esprima': '~1.0.2',
        'estraverse': '~1.3.0'
      },
      'optionalDependencies': { 'source-map': '>= 0.1.2' },
      'devDependencies': {
        'esprima-moz': '*',
        'commonjs-everywhere': '~0.8.0',
        'q': '*',
        'bower': '*',
        'semver': '*',
        'chai': '~1.7.2',
        'grunt-contrib-jshint': '~0.5.0',
        'grunt-cli': '~0.1.9',
        'grunt': '~0.4.1',
        'grunt-mocha-test': '~0.6.2'
      },
      'licenses': [{
          'type': 'BSD',
          'url': 'http://github.com/Constellation/escodegen/raw/master/LICENSE.BSD'
        }],
      'scripts': {
        'test': 'grunt travis',
        'unit-test': 'grunt test',
        'lint': 'grunt lint',
        'release': 'node tools/release.js',
        'build-min': './node_modules/.bin/cjsify -ma path: tools/entry-point.js > escodegen.browser.min.js',
        'build': './node_modules/.bin/cjsify -a path: tools/entry-point.js > escodegen.browser.js'
      }
    };
  });
  require.define('/node_modules/source-map/lib/source-map.js', function (module, exports, __dirname, __filename) {
    exports.SourceMapGenerator = require('/node_modules/source-map/lib/source-map/source-map-generator.js', module).SourceMapGenerator;
    exports.SourceMapConsumer = require('/node_modules/source-map/lib/source-map/source-map-consumer.js', module).SourceMapConsumer;
    exports.SourceNode = require('/node_modules/source-map/lib/source-map/source-node.js', module).SourceNode;
  });
  require.define('/node_modules/source-map/lib/source-map/source-node.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var SourceMapGenerator = require('/node_modules/source-map/lib/source-map/source-map-generator.js', module).SourceMapGenerator;
      var util = require('/node_modules/source-map/lib/source-map/util.js', module);
      function SourceNode(aLine, aColumn, aSource, aChunks, aName) {
        this.children = [];
        this.sourceContents = {};
        this.line = aLine === undefined ? null : aLine;
        this.column = aColumn === undefined ? null : aColumn;
        this.source = aSource === undefined ? null : aSource;
        this.name = aName === undefined ? null : aName;
        if (aChunks != null)
          this.add(aChunks);
      }
      SourceNode.fromStringWithSourceMap = function SourceNode_fromStringWithSourceMap(aGeneratedCode, aSourceMapConsumer) {
        var node = new SourceNode;
        var remainingLines = aGeneratedCode.split('\n');
        var lastGeneratedLine = 1, lastGeneratedColumn = 0;
        var lastMapping = null;
        aSourceMapConsumer.eachMapping(function (mapping) {
          if (lastMapping === null) {
            while (lastGeneratedLine < mapping.generatedLine) {
              node.add(remainingLines.shift() + '\n');
              lastGeneratedLine++;
            }
            if (lastGeneratedColumn < mapping.generatedColumn) {
              var nextLine = remainingLines[0];
              node.add(nextLine.substr(0, mapping.generatedColumn));
              remainingLines[0] = nextLine.substr(mapping.generatedColumn);
              lastGeneratedColumn = mapping.generatedColumn;
            }
          } else {
            if (lastGeneratedLine < mapping.generatedLine) {
              var code = '';
              do {
                code += remainingLines.shift() + '\n';
                lastGeneratedLine++;
                lastGeneratedColumn = 0;
              } while (lastGeneratedLine < mapping.generatedLine);
              if (lastGeneratedColumn < mapping.generatedColumn) {
                var nextLine = remainingLines[0];
                code += nextLine.substr(0, mapping.generatedColumn);
                remainingLines[0] = nextLine.substr(mapping.generatedColumn);
                lastGeneratedColumn = mapping.generatedColumn;
              }
              addMappingWithCode(lastMapping, code);
            } else {
              var nextLine = remainingLines[0];
              var code = nextLine.substr(0, mapping.generatedColumn - lastGeneratedColumn);
              remainingLines[0] = nextLine.substr(mapping.generatedColumn - lastGeneratedColumn);
              lastGeneratedColumn = mapping.generatedColumn;
              addMappingWithCode(lastMapping, code);
            }
          }
          lastMapping = mapping;
        }, this);
        addMappingWithCode(lastMapping, remainingLines.join('\n'));
        aSourceMapConsumer.sources.forEach(function (sourceFile) {
          var content = aSourceMapConsumer.sourceContentFor(sourceFile);
          if (content) {
            node.setSourceContent(sourceFile, content);
          }
        });
        return node;
        function addMappingWithCode(mapping, code) {
          if (mapping === null || mapping.source === undefined) {
            node.add(code);
          } else {
            node.add(new SourceNode(mapping.originalLine, mapping.originalColumn, mapping.source, code, mapping.name));
          }
        }
      };
      SourceNode.prototype.add = function SourceNode_add(aChunk) {
        if (Array.isArray(aChunk)) {
          aChunk.forEach(function (chunk) {
            this.add(chunk);
          }, this);
        } else if (aChunk instanceof SourceNode || typeof aChunk === 'string') {
          if (aChunk) {
            this.children.push(aChunk);
          }
        } else {
          throw new TypeError('Expected a SourceNode, string, or an array of SourceNodes and strings. Got ' + aChunk);
        }
        return this;
      };
      SourceNode.prototype.prepend = function SourceNode_prepend(aChunk) {
        if (Array.isArray(aChunk)) {
          for (var i = aChunk.length - 1; i >= 0; i--) {
            this.prepend(aChunk[i]);
          }
        } else if (aChunk instanceof SourceNode || typeof aChunk === 'string') {
          this.children.unshift(aChunk);
        } else {
          throw new TypeError('Expected a SourceNode, string, or an array of SourceNodes and strings. Got ' + aChunk);
        }
        return this;
      };
      SourceNode.prototype.walk = function SourceNode_walk(aFn) {
        this.children.forEach(function (chunk) {
          if (chunk instanceof SourceNode) {
            chunk.walk(aFn);
          } else {
            if (chunk !== '') {
              aFn(chunk, {
                source: this.source,
                line: this.line,
                column: this.column,
                name: this.name
              });
            }
          }
        }, this);
      };
      SourceNode.prototype.join = function SourceNode_join(aSep) {
        var newChildren;
        var i;
        var len = this.children.length;
        if (len > 0) {
          newChildren = [];
          for (i = 0; i < len - 1; i++) {
            newChildren.push(this.children[i]);
            newChildren.push(aSep);
          }
          newChildren.push(this.children[i]);
          this.children = newChildren;
        }
        return this;
      };
      SourceNode.prototype.replaceRight = function SourceNode_replaceRight(aPattern, aReplacement) {
        var lastChild = this.children[this.children.length - 1];
        if (lastChild instanceof SourceNode) {
          lastChild.replaceRight(aPattern, aReplacement);
        } else if (typeof lastChild === 'string') {
          this.children[this.children.length - 1] = lastChild.replace(aPattern, aReplacement);
        } else {
          this.children.push(''.replace(aPattern, aReplacement));
        }
        return this;
      };
      SourceNode.prototype.setSourceContent = function SourceNode_setSourceContent(aSourceFile, aSourceContent) {
        this.sourceContents[util.toSetString(aSourceFile)] = aSourceContent;
      };
      SourceNode.prototype.walkSourceContents = function SourceNode_walkSourceContents(aFn) {
        this.children.forEach(function (chunk) {
          if (chunk instanceof SourceNode) {
            chunk.walkSourceContents(aFn);
          }
        }, this);
        Object.keys(this.sourceContents).forEach(function (sourceFileKey) {
          aFn(util.fromSetString(sourceFileKey), this.sourceContents[sourceFileKey]);
        }, this);
      };
      SourceNode.prototype.toString = function SourceNode_toString() {
        var str = '';
        this.walk(function (chunk) {
          str += chunk;
        });
        return str;
      };
      SourceNode.prototype.toStringWithSourceMap = function SourceNode_toStringWithSourceMap(aArgs) {
        var generated = {
            code: '',
            line: 1,
            column: 0
          };
        var map = new SourceMapGenerator(aArgs);
        var sourceMappingActive = false;
        var lastOriginalSource = null;
        var lastOriginalLine = null;
        var lastOriginalColumn = null;
        var lastOriginalName = null;
        this.walk(function (chunk, original) {
          generated.code += chunk;
          if (original.source !== null && original.line !== null && original.column !== null) {
            if (lastOriginalSource !== original.source || lastOriginalLine !== original.line || lastOriginalColumn !== original.column || lastOriginalName !== original.name) {
              map.addMapping({
                source: original.source,
                original: {
                  line: original.line,
                  column: original.column
                },
                generated: {
                  line: generated.line,
                  column: generated.column
                },
                name: original.name
              });
            }
            lastOriginalSource = original.source;
            lastOriginalLine = original.line;
            lastOriginalColumn = original.column;
            lastOriginalName = original.name;
            sourceMappingActive = true;
          } else if (sourceMappingActive) {
            map.addMapping({
              generated: {
                line: generated.line,
                column: generated.column
              }
            });
            lastOriginalSource = null;
            sourceMappingActive = false;
          }
          chunk.split('').forEach(function (ch) {
            if (ch === '\n') {
              generated.line++;
              generated.column = 0;
            } else {
              generated.column++;
            }
          });
        });
        this.walkSourceContents(function (sourceFile, sourceContent) {
          map.setSourceContent(sourceFile, sourceContent);
        });
        return {
          code: generated.code,
          map: map
        };
      };
      exports.SourceNode = SourceNode;
    });
  });
  require.define('/node_modules/source-map/lib/source-map/util.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      function getArg(aArgs, aName, aDefaultValue) {
        if (aName in aArgs) {
          return aArgs[aName];
        } else if (arguments.length === 3) {
          return aDefaultValue;
        } else {
          throw new Error('"' + aName + '" is a required argument.');
        }
      }
      exports.getArg = getArg;
      var urlRegexp = /([\w+\-.]+):\/\/((\w+:\w+)@)?([\w.]+)?(:(\d+))?(\S+)?/;
      function urlParse(aUrl) {
        var match = aUrl.match(urlRegexp);
        if (!match) {
          return null;
        }
        return {
          scheme: match[1],
          auth: match[3],
          host: match[4],
          port: match[6],
          path: match[7]
        };
      }
      exports.urlParse = urlParse;
      function urlGenerate(aParsedUrl) {
        var url = aParsedUrl.scheme + '://';
        if (aParsedUrl.auth) {
          url += aParsedUrl.auth + '@';
        }
        if (aParsedUrl.host) {
          url += aParsedUrl.host;
        }
        if (aParsedUrl.port) {
          url += ':' + aParsedUrl.port;
        }
        if (aParsedUrl.path) {
          url += aParsedUrl.path;
        }
        return url;
      }
      exports.urlGenerate = urlGenerate;
      function join(aRoot, aPath) {
        var url;
        if (aPath.match(urlRegexp)) {
          return aPath;
        }
        if (aPath.charAt(0) === '/' && (url = urlParse(aRoot))) {
          url.path = aPath;
          return urlGenerate(url);
        }
        return aRoot.replace(/\/$/, '') + '/' + aPath;
      }
      exports.join = join;
      function toSetString(aStr) {
        return '$' + aStr;
      }
      exports.toSetString = toSetString;
      function fromSetString(aStr) {
        return aStr.substr(1);
      }
      exports.fromSetString = fromSetString;
      function relative(aRoot, aPath) {
        aRoot = aRoot.replace(/\/$/, '');
        var url = urlParse(aRoot);
        if (aPath.charAt(0) == '/' && url && url.path == '/') {
          return aPath.slice(1);
        }
        return aPath.indexOf(aRoot + '/') === 0 ? aPath.substr(aRoot.length + 1) : aPath;
      }
      exports.relative = relative;
    });
  });
  require.define('/node_modules/source-map/node_modules/amdefine/amdefine.js', function (module, exports, __dirname, __filename) {
    'use strict';
    function amdefine(module, requireFn) {
      'use strict';
      var defineCache = {}, loaderCache = {}, alreadyCalled = false, path = require('path', module), makeRequire, stringRequire;
      function trimDots(ary) {
        var i, part;
        for (i = 0; ary[i]; i += 1) {
          part = ary[i];
          if (part === '.') {
            ary.splice(i, 1);
            i -= 1;
          } else if (part === '..') {
            if (i === 1 && (ary[2] === '..' || ary[0] === '..')) {
              break;
            } else if (i > 0) {
              ary.splice(i - 1, 2);
              i -= 2;
            }
          }
        }
      }
      function normalize(name, baseName) {
        var baseParts;
        if (name && name.charAt(0) === '.') {
          if (baseName) {
            baseParts = baseName.split('/');
            baseParts = baseParts.slice(0, baseParts.length - 1);
            baseParts = baseParts.concat(name.split('/'));
            trimDots(baseParts);
            name = baseParts.join('/');
          }
        }
        return name;
      }
      function makeNormalize(relName) {
        return function (name) {
          return normalize(name, relName);
        };
      }
      function makeLoad(id) {
        function load(value) {
          loaderCache[id] = value;
        }
        load.fromText = function (id, text) {
          throw new Error('amdefine does not implement load.fromText');
        };
        return load;
      }
      makeRequire = function (systemRequire, exports, module, relId) {
        function amdRequire(deps, callback) {
          if (typeof deps === 'string') {
            return stringRequire(systemRequire, exports, module, deps, relId);
          } else {
            deps = deps.map(function (depName) {
              return stringRequire(systemRequire, exports, module, depName, relId);
            });
            process.nextTick(function () {
              callback.apply(null, deps);
            });
          }
        }
        amdRequire.toUrl = function (filePath) {
          if (filePath.indexOf('.') === 0) {
            return normalize(filePath, path.dirname(module.filename));
          } else {
            return filePath;
          }
        };
        return amdRequire;
      };
      requireFn = requireFn || function req() {
        return module.require.apply(module, arguments);
      };
      function runFactory(id, deps, factory) {
        var r, e, m, result;
        if (id) {
          e = loaderCache[id] = {};
          m = {
            id: id,
            uri: __filename,
            exports: e
          };
          r = makeRequire(requireFn, e, m, id);
        } else {
          if (alreadyCalled) {
            throw new Error('amdefine with no module ID cannot be called more than once per file.');
          }
          alreadyCalled = true;
          e = module.exports;
          m = module;
          r = makeRequire(requireFn, e, m, module.id);
        }
        if (deps) {
          deps = deps.map(function (depName) {
            return r(depName);
          });
        }
        if (typeof factory === 'function') {
          result = factory.apply(m.exports, deps);
        } else {
          result = factory;
        }
        if (result !== undefined) {
          m.exports = result;
          if (id) {
            loaderCache[id] = m.exports;
          }
        }
      }
      stringRequire = function (systemRequire, exports, module, id, relId) {
        var index = id.indexOf('!'), originalId = id, prefix, plugin;
        if (index === -1) {
          id = normalize(id, relId);
          if (id === 'require') {
            return makeRequire(systemRequire, exports, module, relId);
          } else if (id === 'exports') {
            return exports;
          } else if (id === 'module') {
            return module;
          } else if (loaderCache.hasOwnProperty(id)) {
            return loaderCache[id];
          } else if (defineCache[id]) {
            runFactory.apply(null, defineCache[id]);
            return loaderCache[id];
          } else {
            if (systemRequire) {
              return systemRequire(originalId);
            } else {
              throw new Error('No module with ID: ' + id);
            }
          }
        } else {
          prefix = id.substring(0, index);
          id = id.substring(index + 1, id.length);
          plugin = stringRequire(systemRequire, exports, module, prefix, relId);
          if (plugin.normalize) {
            id = plugin.normalize(id, makeNormalize(relId));
          } else {
            id = normalize(id, relId);
          }
          if (loaderCache[id]) {
            return loaderCache[id];
          } else {
            plugin.load(id, makeRequire(systemRequire, exports, module, relId), makeLoad(id), {});
            return loaderCache[id];
          }
        }
      };
      function define(id, deps, factory) {
        if (Array.isArray(id)) {
          factory = deps;
          deps = id;
          id = undefined;
        } else if (typeof id !== 'string') {
          factory = id;
          id = deps = undefined;
        }
        if (deps && !Array.isArray(deps)) {
          factory = deps;
          deps = undefined;
        }
        if (!deps) {
          deps = [
            'require',
            'exports',
            'module'
          ];
        }
        if (id) {
          defineCache[id] = [
            id,
            deps,
            factory
          ];
        } else {
          runFactory(id, deps, factory);
        }
      }
      define.require = function (id) {
        if (loaderCache[id]) {
          return loaderCache[id];
        }
        if (defineCache[id]) {
          runFactory.apply(null, defineCache[id]);
          return loaderCache[id];
        }
      };
      define.amd = {};
      return define;
    }
    module.exports = amdefine;
  });
  require.define('/node_modules/source-map/lib/source-map/source-map-generator.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var base64VLQ = require('/node_modules/source-map/lib/source-map/base64-vlq.js', module);
      var util = require('/node_modules/source-map/lib/source-map/util.js', module);
      var ArraySet = require('/node_modules/source-map/lib/source-map/array-set.js', module).ArraySet;
      function SourceMapGenerator(aArgs) {
        this._file = util.getArg(aArgs, 'file');
        this._sourceRoot = util.getArg(aArgs, 'sourceRoot', null);
        this._sources = new ArraySet;
        this._names = new ArraySet;
        this._mappings = [];
        this._sourcesContents = null;
      }
      SourceMapGenerator.prototype._version = 3;
      SourceMapGenerator.fromSourceMap = function SourceMapGenerator_fromSourceMap(aSourceMapConsumer) {
        var sourceRoot = aSourceMapConsumer.sourceRoot;
        var generator = new SourceMapGenerator({
            file: aSourceMapConsumer.file,
            sourceRoot: sourceRoot
          });
        aSourceMapConsumer.eachMapping(function (mapping) {
          var newMapping = {
              generated: {
                line: mapping.generatedLine,
                column: mapping.generatedColumn
              }
            };
          if (mapping.source) {
            newMapping.source = mapping.source;
            if (sourceRoot) {
              newMapping.source = util.relative(sourceRoot, newMapping.source);
            }
            newMapping.original = {
              line: mapping.originalLine,
              column: mapping.originalColumn
            };
            if (mapping.name) {
              newMapping.name = mapping.name;
            }
          }
          generator.addMapping(newMapping);
        });
        aSourceMapConsumer.sources.forEach(function (sourceFile) {
          var content = aSourceMapConsumer.sourceContentFor(sourceFile);
          if (content) {
            generator.setSourceContent(sourceFile, content);
          }
        });
        return generator;
      };
      SourceMapGenerator.prototype.addMapping = function SourceMapGenerator_addMapping(aArgs) {
        var generated = util.getArg(aArgs, 'generated');
        var original = util.getArg(aArgs, 'original', null);
        var source = util.getArg(aArgs, 'source', null);
        var name = util.getArg(aArgs, 'name', null);
        this._validateMapping(generated, original, source, name);
        if (source && !this._sources.has(source)) {
          this._sources.add(source);
        }
        if (name && !this._names.has(name)) {
          this._names.add(name);
        }
        this._mappings.push({
          generated: generated,
          original: original,
          source: source,
          name: name
        });
      };
      SourceMapGenerator.prototype.setSourceContent = function SourceMapGenerator_setSourceContent(aSourceFile, aSourceContent) {
        var source = aSourceFile;
        if (this._sourceRoot) {
          source = util.relative(this._sourceRoot, source);
        }
        if (aSourceContent !== null) {
          if (!this._sourcesContents) {
            this._sourcesContents = {};
          }
          this._sourcesContents[util.toSetString(source)] = aSourceContent;
        } else {
          delete this._sourcesContents[util.toSetString(source)];
          if (Object.keys(this._sourcesContents).length === 0) {
            this._sourcesContents = null;
          }
        }
      };
      SourceMapGenerator.prototype.applySourceMap = function SourceMapGenerator_applySourceMap(aSourceMapConsumer, aSourceFile) {
        if (!aSourceFile) {
          aSourceFile = aSourceMapConsumer.file;
        }
        var sourceRoot = this._sourceRoot;
        if (sourceRoot) {
          aSourceFile = util.relative(sourceRoot, aSourceFile);
        }
        var newSources = new ArraySet;
        var newNames = new ArraySet;
        this._mappings.forEach(function (mapping) {
          if (mapping.source === aSourceFile && mapping.original) {
            var original = aSourceMapConsumer.originalPositionFor({
                line: mapping.original.line,
                column: mapping.original.column
              });
            if (original.source !== null) {
              if (sourceRoot) {
                mapping.source = util.relative(sourceRoot, original.source);
              } else {
                mapping.source = original.source;
              }
              mapping.original.line = original.line;
              mapping.original.column = original.column;
              if (original.name !== null && mapping.name !== null) {
                mapping.name = original.name;
              }
            }
          }
          var source = mapping.source;
          if (source && !newSources.has(source)) {
            newSources.add(source);
          }
          var name = mapping.name;
          if (name && !newNames.has(name)) {
            newNames.add(name);
          }
        }, this);
        this._sources = newSources;
        this._names = newNames;
        aSourceMapConsumer.sources.forEach(function (sourceFile) {
          var content = aSourceMapConsumer.sourceContentFor(sourceFile);
          if (content) {
            if (sourceRoot) {
              sourceFile = util.relative(sourceRoot, sourceFile);
            }
            this.setSourceContent(sourceFile, content);
          }
        }, this);
      };
      SourceMapGenerator.prototype._validateMapping = function SourceMapGenerator_validateMapping(aGenerated, aOriginal, aSource, aName) {
        if (aGenerated && 'line' in aGenerated && 'column' in aGenerated && aGenerated.line > 0 && aGenerated.column >= 0 && !aOriginal && !aSource && !aName) {
          return;
        } else if (aGenerated && 'line' in aGenerated && 'column' in aGenerated && aOriginal && 'line' in aOriginal && 'column' in aOriginal && aGenerated.line > 0 && aGenerated.column >= 0 && aOriginal.line > 0 && aOriginal.column >= 0 && aSource) {
          return;
        } else {
          throw new Error('Invalid mapping.');
        }
      };
      function cmpLocation(loc1, loc2) {
        var cmp = (loc1 && loc1.line) - (loc2 && loc2.line);
        return cmp ? cmp : (loc1 && loc1.column) - (loc2 && loc2.column);
      }
      function strcmp(str1, str2) {
        str1 = str1 || '';
        str2 = str2 || '';
        return (str1 > str2) - (str1 < str2);
      }
      function cmpMapping(mappingA, mappingB) {
        return cmpLocation(mappingA.generated, mappingB.generated) || cmpLocation(mappingA.original, mappingB.original) || strcmp(mappingA.source, mappingB.source) || strcmp(mappingA.name, mappingB.name);
      }
      SourceMapGenerator.prototype._serializeMappings = function SourceMapGenerator_serializeMappings() {
        var previousGeneratedColumn = 0;
        var previousGeneratedLine = 1;
        var previousOriginalColumn = 0;
        var previousOriginalLine = 0;
        var previousName = 0;
        var previousSource = 0;
        var result = '';
        var mapping;
        this._mappings.sort(cmpMapping);
        for (var i = 0, len = this._mappings.length; i < len; i++) {
          mapping = this._mappings[i];
          if (mapping.generated.line !== previousGeneratedLine) {
            previousGeneratedColumn = 0;
            while (mapping.generated.line !== previousGeneratedLine) {
              result += ';';
              previousGeneratedLine++;
            }
          } else {
            if (i > 0) {
              if (!cmpMapping(mapping, this._mappings[i - 1])) {
                continue;
              }
              result += ',';
            }
          }
          result += base64VLQ.encode(mapping.generated.column - previousGeneratedColumn);
          previousGeneratedColumn = mapping.generated.column;
          if (mapping.source && mapping.original) {
            result += base64VLQ.encode(this._sources.indexOf(mapping.source) - previousSource);
            previousSource = this._sources.indexOf(mapping.source);
            result += base64VLQ.encode(mapping.original.line - 1 - previousOriginalLine);
            previousOriginalLine = mapping.original.line - 1;
            result += base64VLQ.encode(mapping.original.column - previousOriginalColumn);
            previousOriginalColumn = mapping.original.column;
            if (mapping.name) {
              result += base64VLQ.encode(this._names.indexOf(mapping.name) - previousName);
              previousName = this._names.indexOf(mapping.name);
            }
          }
        }
        return result;
      };
      SourceMapGenerator.prototype.toJSON = function SourceMapGenerator_toJSON() {
        var map = {
            version: this._version,
            file: this._file,
            sources: this._sources.toArray(),
            names: this._names.toArray(),
            mappings: this._serializeMappings()
          };
        if (this._sourceRoot) {
          map.sourceRoot = this._sourceRoot;
        }
        if (this._sourcesContents) {
          map.sourcesContent = map.sources.map(function (source) {
            if (map.sourceRoot) {
              source = util.relative(map.sourceRoot, source);
            }
            return Object.prototype.hasOwnProperty.call(this._sourcesContents, util.toSetString(source)) ? this._sourcesContents[util.toSetString(source)] : null;
          }, this);
        }
        return map;
      };
      SourceMapGenerator.prototype.toString = function SourceMapGenerator_toString() {
        return JSON.stringify(this);
      };
      exports.SourceMapGenerator = SourceMapGenerator;
    });
  });
  require.define('/node_modules/source-map/lib/source-map/array-set.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var util = require('/node_modules/source-map/lib/source-map/util.js', module);
      function ArraySet() {
        this._array = [];
        this._set = {};
      }
      ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
        var set = new ArraySet;
        for (var i = 0, len = aArray.length; i < len; i++) {
          set.add(aArray[i], aAllowDuplicates);
        }
        return set;
      };
      ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
        var isDuplicate = this.has(aStr);
        var idx = this._array.length;
        if (!isDuplicate || aAllowDuplicates) {
          this._array.push(aStr);
        }
        if (!isDuplicate) {
          this._set[util.toSetString(aStr)] = idx;
        }
      };
      ArraySet.prototype.has = function ArraySet_has(aStr) {
        return Object.prototype.hasOwnProperty.call(this._set, util.toSetString(aStr));
      };
      ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
        if (this.has(aStr)) {
          return this._set[util.toSetString(aStr)];
        }
        throw new Error('"' + aStr + '" is not in the set.');
      };
      ArraySet.prototype.at = function ArraySet_at(aIdx) {
        if (aIdx >= 0 && aIdx < this._array.length) {
          return this._array[aIdx];
        }
        throw new Error('No element indexed by ' + aIdx);
      };
      ArraySet.prototype.toArray = function ArraySet_toArray() {
        return this._array.slice();
      };
      exports.ArraySet = ArraySet;
    });
  });
  require.define('/node_modules/source-map/lib/source-map/base64-vlq.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var base64 = require('/node_modules/source-map/lib/source-map/base64.js', module);
      var VLQ_BASE_SHIFT = 5;
      var VLQ_BASE = 1 << VLQ_BASE_SHIFT;
      var VLQ_BASE_MASK = VLQ_BASE - 1;
      var VLQ_CONTINUATION_BIT = VLQ_BASE;
      function toVLQSigned(aValue) {
        return aValue < 0 ? (-aValue << 1) + 1 : (aValue << 1) + 0;
      }
      function fromVLQSigned(aValue) {
        var isNegative = (aValue & 1) === 1;
        var shifted = aValue >> 1;
        return isNegative ? -shifted : shifted;
      }
      exports.encode = function base64VLQ_encode(aValue) {
        var encoded = '';
        var digit;
        var vlq = toVLQSigned(aValue);
        do {
          digit = vlq & VLQ_BASE_MASK;
          vlq >>>= VLQ_BASE_SHIFT;
          if (vlq > 0) {
            digit |= VLQ_CONTINUATION_BIT;
          }
          encoded += base64.encode(digit);
        } while (vlq > 0);
        return encoded;
      };
      exports.decode = function base64VLQ_decode(aStr) {
        var i = 0;
        var strLen = aStr.length;
        var result = 0;
        var shift = 0;
        var continuation, digit;
        do {
          if (i >= strLen) {
            throw new Error('Expected more digits in base 64 VLQ value.');
          }
          digit = base64.decode(aStr.charAt(i++));
          continuation = !!(digit & VLQ_CONTINUATION_BIT);
          digit &= VLQ_BASE_MASK;
          result = result + (digit << shift);
          shift += VLQ_BASE_SHIFT;
        } while (continuation);
        return {
          value: fromVLQSigned(result),
          rest: aStr.slice(i)
        };
      };
    });
  });
  require.define('/node_modules/source-map/lib/source-map/base64.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var charToIntMap = {};
      var intToCharMap = {};
      'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'.split('').forEach(function (ch, index) {
        charToIntMap[ch] = index;
        intToCharMap[index] = ch;
      });
      exports.encode = function base64_encode(aNumber) {
        if (aNumber in intToCharMap) {
          return intToCharMap[aNumber];
        }
        throw new TypeError('Must be between 0 and 63: ' + aNumber);
      };
      exports.decode = function base64_decode(aChar) {
        if (aChar in charToIntMap) {
          return charToIntMap[aChar];
        }
        throw new TypeError('Not a valid base 64 digit: ' + aChar);
      };
    });
  });
  require.define('/node_modules/source-map/lib/source-map/source-map-consumer.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      var util = require('/node_modules/source-map/lib/source-map/util.js', module);
      var binarySearch = require('/node_modules/source-map/lib/source-map/binary-search.js', module);
      var ArraySet = require('/node_modules/source-map/lib/source-map/array-set.js', module).ArraySet;
      var base64VLQ = require('/node_modules/source-map/lib/source-map/base64-vlq.js', module);
      function SourceMapConsumer(aSourceMap) {
        var sourceMap = aSourceMap;
        if (typeof aSourceMap === 'string') {
          sourceMap = JSON.parse(aSourceMap.replace(/^\)\]\}'/, ''));
        }
        var version = util.getArg(sourceMap, 'version');
        var sources = util.getArg(sourceMap, 'sources');
        var names = util.getArg(sourceMap, 'names');
        var sourceRoot = util.getArg(sourceMap, 'sourceRoot', null);
        var sourcesContent = util.getArg(sourceMap, 'sourcesContent', null);
        var mappings = util.getArg(sourceMap, 'mappings');
        var file = util.getArg(sourceMap, 'file', null);
        if (version !== this._version) {
          throw new Error('Unsupported version: ' + version);
        }
        this._names = ArraySet.fromArray(names, true);
        this._sources = ArraySet.fromArray(sources, true);
        this.sourceRoot = sourceRoot;
        this.sourcesContent = sourcesContent;
        this.file = file;
        this._generatedMappings = [];
        this._originalMappings = [];
        this._parseMappings(mappings, sourceRoot);
      }
      SourceMapConsumer.prototype._version = 3;
      Object.defineProperty(SourceMapConsumer.prototype, 'sources', {
        get: function () {
          return this._sources.toArray().map(function (s) {
            return this.sourceRoot ? util.join(this.sourceRoot, s) : s;
          }, this);
        }
      });
      SourceMapConsumer.prototype._parseMappings = function SourceMapConsumer_parseMappings(aStr, aSourceRoot) {
        var generatedLine = 1;
        var previousGeneratedColumn = 0;
        var previousOriginalLine = 0;
        var previousOriginalColumn = 0;
        var previousSource = 0;
        var previousName = 0;
        var mappingSeparator = /^[,;]/;
        var str = aStr;
        var mapping;
        var temp;
        while (str.length > 0) {
          if (str.charAt(0) === ';') {
            generatedLine++;
            str = str.slice(1);
            previousGeneratedColumn = 0;
          } else if (str.charAt(0) === ',') {
            str = str.slice(1);
          } else {
            mapping = {};
            mapping.generatedLine = generatedLine;
            temp = base64VLQ.decode(str);
            mapping.generatedColumn = previousGeneratedColumn + temp.value;
            previousGeneratedColumn = mapping.generatedColumn;
            str = temp.rest;
            if (str.length > 0 && !mappingSeparator.test(str.charAt(0))) {
              temp = base64VLQ.decode(str);
              mapping.source = this._sources.at(previousSource + temp.value);
              previousSource += temp.value;
              str = temp.rest;
              if (str.length === 0 || mappingSeparator.test(str.charAt(0))) {
                throw new Error('Found a source, but no line and column');
              }
              temp = base64VLQ.decode(str);
              mapping.originalLine = previousOriginalLine + temp.value;
              previousOriginalLine = mapping.originalLine;
              mapping.originalLine += 1;
              str = temp.rest;
              if (str.length === 0 || mappingSeparator.test(str.charAt(0))) {
                throw new Error('Found a source and line, but no column');
              }
              temp = base64VLQ.decode(str);
              mapping.originalColumn = previousOriginalColumn + temp.value;
              previousOriginalColumn = mapping.originalColumn;
              str = temp.rest;
              if (str.length > 0 && !mappingSeparator.test(str.charAt(0))) {
                temp = base64VLQ.decode(str);
                mapping.name = this._names.at(previousName + temp.value);
                previousName += temp.value;
                str = temp.rest;
              }
            }
            this._generatedMappings.push(mapping);
            if (typeof mapping.originalLine === 'number') {
              this._originalMappings.push(mapping);
            }
          }
        }
        this._originalMappings.sort(this._compareOriginalPositions);
      };
      SourceMapConsumer.prototype._compareOriginalPositions = function SourceMapConsumer_compareOriginalPositions(mappingA, mappingB) {
        if (mappingA.source > mappingB.source) {
          return 1;
        } else if (mappingA.source < mappingB.source) {
          return -1;
        } else {
          var cmp = mappingA.originalLine - mappingB.originalLine;
          return cmp === 0 ? mappingA.originalColumn - mappingB.originalColumn : cmp;
        }
      };
      SourceMapConsumer.prototype._compareGeneratedPositions = function SourceMapConsumer_compareGeneratedPositions(mappingA, mappingB) {
        var cmp = mappingA.generatedLine - mappingB.generatedLine;
        return cmp === 0 ? mappingA.generatedColumn - mappingB.generatedColumn : cmp;
      };
      SourceMapConsumer.prototype._findMapping = function SourceMapConsumer_findMapping(aNeedle, aMappings, aLineName, aColumnName, aComparator) {
        if (aNeedle[aLineName] <= 0) {
          throw new TypeError('Line must be greater than or equal to 1, got ' + aNeedle[aLineName]);
        }
        if (aNeedle[aColumnName] < 0) {
          throw new TypeError('Column must be greater than or equal to 0, got ' + aNeedle[aColumnName]);
        }
        return binarySearch.search(aNeedle, aMappings, aComparator);
      };
      SourceMapConsumer.prototype.originalPositionFor = function SourceMapConsumer_originalPositionFor(aArgs) {
        var needle = {
            generatedLine: util.getArg(aArgs, 'line'),
            generatedColumn: util.getArg(aArgs, 'column')
          };
        var mapping = this._findMapping(needle, this._generatedMappings, 'generatedLine', 'generatedColumn', this._compareGeneratedPositions);
        if (mapping) {
          var source = util.getArg(mapping, 'source', null);
          if (source && this.sourceRoot) {
            source = util.join(this.sourceRoot, source);
          }
          return {
            source: source,
            line: util.getArg(mapping, 'originalLine', null),
            column: util.getArg(mapping, 'originalColumn', null),
            name: util.getArg(mapping, 'name', null)
          };
        }
        return {
          source: null,
          line: null,
          column: null,
          name: null
        };
      };
      SourceMapConsumer.prototype.sourceContentFor = function SourceMapConsumer_sourceContentFor(aSource) {
        if (!this.sourcesContent) {
          return null;
        }
        if (this.sourceRoot) {
          aSource = util.relative(this.sourceRoot, aSource);
        }
        if (this._sources.has(aSource)) {
          return this.sourcesContent[this._sources.indexOf(aSource)];
        }
        var url;
        if (this.sourceRoot && (url = util.urlParse(this.sourceRoot))) {
          var fileUriAbsPath = aSource.replace(/^file:\/\//, '');
          if (url.scheme == 'file' && this._sources.has(fileUriAbsPath)) {
            return this.sourcesContent[this._sources.indexOf(fileUriAbsPath)];
          }
          if ((!url.path || url.path == '/') && this._sources.has('/' + aSource)) {
            return this.sourcesContent[this._sources.indexOf('/' + aSource)];
          }
        }
        throw new Error('"' + aSource + '" is not in the SourceMap.');
      };
      SourceMapConsumer.prototype.generatedPositionFor = function SourceMapConsumer_generatedPositionFor(aArgs) {
        var needle = {
            source: util.getArg(aArgs, 'source'),
            originalLine: util.getArg(aArgs, 'line'),
            originalColumn: util.getArg(aArgs, 'column')
          };
        if (this.sourceRoot) {
          needle.source = util.relative(this.sourceRoot, needle.source);
        }
        var mapping = this._findMapping(needle, this._originalMappings, 'originalLine', 'originalColumn', this._compareOriginalPositions);
        if (mapping) {
          return {
            line: util.getArg(mapping, 'generatedLine', null),
            column: util.getArg(mapping, 'generatedColumn', null)
          };
        }
        return {
          line: null,
          column: null
        };
      };
      SourceMapConsumer.GENERATED_ORDER = 1;
      SourceMapConsumer.ORIGINAL_ORDER = 2;
      SourceMapConsumer.prototype.eachMapping = function SourceMapConsumer_eachMapping(aCallback, aContext, aOrder) {
        var context = aContext || null;
        var order = aOrder || SourceMapConsumer.GENERATED_ORDER;
        var mappings;
        switch (order) {
        case SourceMapConsumer.GENERATED_ORDER:
          mappings = this._generatedMappings;
          break;
        case SourceMapConsumer.ORIGINAL_ORDER:
          mappings = this._originalMappings;
          break;
        default:
          throw new Error('Unknown order of iteration.');
        }
        var sourceRoot = this.sourceRoot;
        mappings.map(function (mapping) {
          var source = mapping.source;
          if (source && sourceRoot) {
            source = util.join(sourceRoot, source);
          }
          return {
            source: source,
            generatedLine: mapping.generatedLine,
            generatedColumn: mapping.generatedColumn,
            originalLine: mapping.originalLine,
            originalColumn: mapping.originalColumn,
            name: mapping.name
          };
        }).forEach(aCallback, context);
      };
      exports.SourceMapConsumer = SourceMapConsumer;
    });
  });
  require.define('/node_modules/source-map/lib/source-map/binary-search.js', function (module, exports, __dirname, __filename) {
    if (typeof define !== 'function') {
      var define = require('/node_modules/source-map/node_modules/amdefine/amdefine.js', module)(module, require);
    }
    define(function (require, exports, module) {
      function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare) {
        var mid = Math.floor((aHigh - aLow) / 2) + aLow;
        var cmp = aCompare(aNeedle, aHaystack[mid]);
        if (cmp === 0) {
          return aHaystack[mid];
        } else if (cmp > 0) {
          if (aHigh - mid > 1) {
            return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare);
          }
          return aHaystack[mid];
        } else {
          if (mid - aLow > 1) {
            return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare);
          }
          return aLow < 0 ? null : aHaystack[aLow];
        }
      }
      exports.search = function search(aNeedle, aHaystack, aCompare) {
        return aHaystack.length > 0 ? recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack, aCompare) : null;
      };
    });
  });
  require.define('/node_modules/estraverse/estraverse.js', function (module, exports, __dirname, __filename) {
    (function (root, factory) {
      'use strict';
      if (typeof define === 'function' && define.amd) {
        define(['exports'], factory);
      } else if (typeof exports !== 'undefined') {
        factory(exports);
      } else {
        factory(root.estraverse = {});
      }
    }(this, function (exports) {
      'use strict';
      var Syntax, isArray, VisitorOption, VisitorKeys, BREAK, SKIP;
      Syntax = {
        AssignmentExpression: 'AssignmentExpression',
        ArrayExpression: 'ArrayExpression',
        ArrowFunctionExpression: 'ArrowFunctionExpression',
        BlockStatement: 'BlockStatement',
        BinaryExpression: 'BinaryExpression',
        BreakStatement: 'BreakStatement',
        CallExpression: 'CallExpression',
        CatchClause: 'CatchClause',
        ConditionalExpression: 'ConditionalExpression',
        ContinueStatement: 'ContinueStatement',
        DebuggerStatement: 'DebuggerStatement',
        DirectiveStatement: 'DirectiveStatement',
        DoWhileStatement: 'DoWhileStatement',
        EmptyStatement: 'EmptyStatement',
        ExpressionStatement: 'ExpressionStatement',
        ForStatement: 'ForStatement',
        ForInStatement: 'ForInStatement',
        FunctionDeclaration: 'FunctionDeclaration',
        FunctionExpression: 'FunctionExpression',
        Identifier: 'Identifier',
        IfStatement: 'IfStatement',
        Literal: 'Literal',
        LabeledStatement: 'LabeledStatement',
        LogicalExpression: 'LogicalExpression',
        MemberExpression: 'MemberExpression',
        NewExpression: 'NewExpression',
        ObjectExpression: 'ObjectExpression',
        Program: 'Program',
        Property: 'Property',
        ReturnStatement: 'ReturnStatement',
        SequenceExpression: 'SequenceExpression',
        SwitchStatement: 'SwitchStatement',
        SwitchCase: 'SwitchCase',
        ThisExpression: 'ThisExpression',
        ThrowStatement: 'ThrowStatement',
        TryStatement: 'TryStatement',
        UnaryExpression: 'UnaryExpression',
        UpdateExpression: 'UpdateExpression',
        VariableDeclaration: 'VariableDeclaration',
        VariableDeclarator: 'VariableDeclarator',
        WhileStatement: 'WhileStatement',
        WithStatement: 'WithStatement',
        YieldExpression: 'YieldExpression'
      };
      function ignoreJSHintError() {
      }
      isArray = Array.isArray;
      if (!isArray) {
        isArray = function isArray(array) {
          return Object.prototype.toString.call(array) === '[object Array]';
        };
      }
      function deepCopy(obj) {
        var ret = {}, key, val;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            val = obj[key];
            if (typeof val === 'object' && val !== null) {
              ret[key] = deepCopy(val);
            } else {
              ret[key] = val;
            }
          }
        }
        return ret;
      }
      function shallowCopy(obj) {
        var ret = {}, key;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            ret[key] = obj[key];
          }
        }
        return ret;
      }
      ignoreJSHintError(shallowCopy);
      function upperBound(array, func) {
        var diff, len, i, current;
        len = array.length;
        i = 0;
        while (len) {
          diff = len >>> 1;
          current = i + diff;
          if (func(array[current])) {
            len = diff;
          } else {
            i = current + 1;
            len -= diff + 1;
          }
        }
        return i;
      }
      function lowerBound(array, func) {
        var diff, len, i, current;
        len = array.length;
        i = 0;
        while (len) {
          diff = len >>> 1;
          current = i + diff;
          if (func(array[current])) {
            i = current + 1;
            len -= diff + 1;
          } else {
            len = diff;
          }
        }
        return i;
      }
      ignoreJSHintError(lowerBound);
      VisitorKeys = {
        AssignmentExpression: [
          'left',
          'right'
        ],
        ArrayExpression: ['elements'],
        ArrowFunctionExpression: [
          'params',
          'body'
        ],
        BlockStatement: ['body'],
        BinaryExpression: [
          'left',
          'right'
        ],
        BreakStatement: ['label'],
        CallExpression: [
          'callee',
          'arguments'
        ],
        CatchClause: [
          'param',
          'body'
        ],
        ConditionalExpression: [
          'test',
          'consequent',
          'alternate'
        ],
        ContinueStatement: ['label'],
        DebuggerStatement: [],
        DirectiveStatement: [],
        DoWhileStatement: [
          'body',
          'test'
        ],
        EmptyStatement: [],
        ExpressionStatement: ['expression'],
        ForStatement: [
          'init',
          'test',
          'update',
          'body'
        ],
        ForInStatement: [
          'left',
          'right',
          'body'
        ],
        FunctionDeclaration: [
          'id',
          'params',
          'body'
        ],
        FunctionExpression: [
          'id',
          'params',
          'body'
        ],
        Identifier: [],
        IfStatement: [
          'test',
          'consequent',
          'alternate'
        ],
        Literal: [],
        LabeledStatement: [
          'label',
          'body'
        ],
        LogicalExpression: [
          'left',
          'right'
        ],
        MemberExpression: [
          'object',
          'property'
        ],
        NewExpression: [
          'callee',
          'arguments'
        ],
        ObjectExpression: ['properties'],
        Program: ['body'],
        Property: [
          'key',
          'value'
        ],
        ReturnStatement: ['argument'],
        SequenceExpression: ['expressions'],
        SwitchStatement: [
          'discriminant',
          'cases'
        ],
        SwitchCase: [
          'test',
          'consequent'
        ],
        ThisExpression: [],
        ThrowStatement: ['argument'],
        TryStatement: [
          'block',
          'handlers',
          'handler',
          'guardedHandlers',
          'finalizer'
        ],
        UnaryExpression: ['argument'],
        UpdateExpression: ['argument'],
        VariableDeclaration: ['declarations'],
        VariableDeclarator: [
          'id',
          'init'
        ],
        WhileStatement: [
          'test',
          'body'
        ],
        WithStatement: [
          'object',
          'body'
        ],
        YieldExpression: ['argument']
      };
      BREAK = {};
      SKIP = {};
      VisitorOption = {
        Break: BREAK,
        Skip: SKIP
      };
      function Reference(parent, key) {
        this.parent = parent;
        this.key = key;
      }
      Reference.prototype.replace = function replace(node) {
        this.parent[this.key] = node;
      };
      function Element(node, path, wrap, ref) {
        this.node = node;
        this.path = path;
        this.wrap = wrap;
        this.ref = ref;
      }
      function Controller() {
      }
      Controller.prototype.path = function path() {
        var i, iz, j, jz, result, element;
        function addToPath(result, path) {
          if (isArray(path)) {
            for (j = 0, jz = path.length; j < jz; ++j) {
              result.push(path[j]);
            }
          } else {
            result.push(path);
          }
        }
        if (!this.__current.path) {
          return null;
        }
        result = [];
        for (i = 2, iz = this.__leavelist.length; i < iz; ++i) {
          element = this.__leavelist[i];
          addToPath(result, element.path);
        }
        addToPath(result, this.__current.path);
        return result;
      };
      Controller.prototype.parents = function parents() {
        var i, iz, result;
        result = [];
        for (i = 1, iz = this.__leavelist.length; i < iz; ++i) {
          result.push(this.__leavelist[i].node);
        }
        return result;
      };
      Controller.prototype.current = function current() {
        return this.__current.node;
      };
      Controller.prototype.__execute = function __execute(callback, element) {
        var previous, result;
        result = undefined;
        previous = this.__current;
        this.__current = element;
        this.__state = null;
        if (callback) {
          result = callback.call(this, element.node, this.__leavelist[this.__leavelist.length - 1].node);
        }
        this.__current = previous;
        return result;
      };
      Controller.prototype.notify = function notify(flag) {
        this.__state = flag;
      };
      Controller.prototype.skip = function () {
        this.notify(SKIP);
      };
      Controller.prototype['break'] = function () {
        this.notify(BREAK);
      };
      Controller.prototype.__initialize = function (root, visitor) {
        this.visitor = visitor;
        this.root = root;
        this.__worklist = [];
        this.__leavelist = [];
        this.__current = null;
        this.__state = null;
      };
      Controller.prototype.traverse = function traverse(root, visitor) {
        var worklist, leavelist, element, node, nodeType, ret, key, current, current2, candidates, candidate, sentinel;
        this.__initialize(root, visitor);
        sentinel = {};
        worklist = this.__worklist;
        leavelist = this.__leavelist;
        worklist.push(new Element(root, null, null, null));
        leavelist.push(new Element(null, null, null, null));
        while (worklist.length) {
          element = worklist.pop();
          if (element === sentinel) {
            element = leavelist.pop();
            ret = this.__execute(visitor.leave, element);
            if (this.__state === BREAK || ret === BREAK) {
              return;
            }
            continue;
          }
          if (element.node) {
            ret = this.__execute(visitor.enter, element);
            if (this.__state === BREAK || ret === BREAK) {
              return;
            }
            worklist.push(sentinel);
            leavelist.push(element);
            if (this.__state === SKIP || ret === SKIP) {
              continue;
            }
            node = element.node;
            nodeType = element.wrap || node.type;
            candidates = VisitorKeys[nodeType];
            current = candidates.length;
            while ((current -= 1) >= 0) {
              key = candidates[current];
              candidate = node[key];
              if (!candidate) {
                continue;
              }
              if (!isArray(candidate)) {
                worklist.push(new Element(candidate, key, null, null));
                continue;
              }
              current2 = candidate.length;
              while ((current2 -= 1) >= 0) {
                if (!candidate[current2]) {
                  continue;
                }
                if (nodeType === Syntax.ObjectExpression && 'properties' === candidates[current]) {
                  element = new Element(candidate[current2], [
                    key,
                    current2
                  ], 'Property', null);
                } else {
                  element = new Element(candidate[current2], [
                    key,
                    current2
                  ], null, null);
                }
                worklist.push(element);
              }
            }
          }
        }
      };
      Controller.prototype.replace = function replace(root, visitor) {
        var worklist, leavelist, node, nodeType, target, element, current, current2, candidates, candidate, sentinel, outer, key;
        this.__initialize(root, visitor);
        sentinel = {};
        worklist = this.__worklist;
        leavelist = this.__leavelist;
        outer = { root: root };
        element = new Element(root, null, null, new Reference(outer, 'root'));
        worklist.push(element);
        leavelist.push(element);
        while (worklist.length) {
          element = worklist.pop();
          if (element === sentinel) {
            element = leavelist.pop();
            target = this.__execute(visitor.leave, element);
            if (target !== undefined && target !== BREAK && target !== SKIP) {
              element.ref.replace(target);
            }
            if (this.__state === BREAK || target === BREAK) {
              return outer.root;
            }
            continue;
          }
          target = this.__execute(visitor.enter, element);
          if (target !== undefined && target !== BREAK && target !== SKIP) {
            element.ref.replace(target);
            element.node = target;
          }
          if (this.__state === BREAK || target === BREAK) {
            return outer.root;
          }
          node = element.node;
          if (!node) {
            continue;
          }
          worklist.push(sentinel);
          leavelist.push(element);
          if (this.__state === SKIP || target === SKIP) {
            continue;
          }
          nodeType = element.wrap || node.type;
          candidates = VisitorKeys[nodeType];
          current = candidates.length;
          while ((current -= 1) >= 0) {
            key = candidates[current];
            candidate = node[key];
            if (!candidate) {
              continue;
            }
            if (!isArray(candidate)) {
              worklist.push(new Element(candidate, key, null, new Reference(node, key)));
              continue;
            }
            current2 = candidate.length;
            while ((current2 -= 1) >= 0) {
              if (!candidate[current2]) {
                continue;
              }
              if (nodeType === Syntax.ObjectExpression && 'properties' === candidates[current]) {
                element = new Element(candidate[current2], [
                  key,
                  current2
                ], 'Property', new Reference(candidate, current2));
              } else {
                element = new Element(candidate[current2], [
                  key,
                  current2
                ], null, new Reference(candidate, current2));
              }
              worklist.push(element);
            }
          }
        }
        return outer.root;
      };
      function traverse(root, visitor) {
        var controller = new Controller;
        return controller.traverse(root, visitor);
      }
      function replace(root, visitor) {
        var controller = new Controller;
        return controller.replace(root, visitor);
      }
      function extendCommentRange(comment, tokens) {
        var target, token;
        target = upperBound(tokens, function search(token) {
          return token.range[0] > comment.range[0];
        });
        comment.extendedRange = [
          comment.range[0],
          comment.range[1]
        ];
        if (target !== tokens.length) {
          comment.extendedRange[1] = tokens[target].range[0];
        }
        target -= 1;
        if (target >= 0) {
          if (target < tokens.length) {
            comment.extendedRange[0] = tokens[target].range[1];
          } else if (token.length) {
            comment.extendedRange[1] = tokens[tokens.length - 1].range[0];
          }
        }
        return comment;
      }
      function attachComments(tree, providedComments, tokens) {
        var comments = [], comment, len, i, cursor;
        if (!tree.range) {
          throw new Error('attachComments needs range information');
        }
        if (!tokens.length) {
          if (providedComments.length) {
            for (i = 0, len = providedComments.length; i < len; i += 1) {
              comment = deepCopy(providedComments[i]);
              comment.extendedRange = [
                0,
                tree.range[0]
              ];
              comments.push(comment);
            }
            tree.leadingComments = comments;
          }
          return tree;
        }
        for (i = 0, len = providedComments.length; i < len; i += 1) {
          comments.push(extendCommentRange(deepCopy(providedComments[i]), tokens));
        }
        cursor = 0;
        traverse(tree, {
          enter: function (node) {
            var comment;
            while (cursor < comments.length) {
              comment = comments[cursor];
              if (comment.extendedRange[1] > node.range[0]) {
                break;
              }
              if (comment.extendedRange[1] === node.range[0]) {
                if (!node.leadingComments) {
                  node.leadingComments = [];
                }
                node.leadingComments.push(comment);
                comments.splice(cursor, 1);
              } else {
                cursor += 1;
              }
            }
            if (cursor === comments.length) {
              return VisitorOption.Break;
            }
            if (comments[cursor].extendedRange[0] > node.range[1]) {
              return VisitorOption.Skip;
            }
          }
        });
        cursor = 0;
        traverse(tree, {
          leave: function (node) {
            var comment;
            while (cursor < comments.length) {
              comment = comments[cursor];
              if (node.range[1] < comment.extendedRange[0]) {
                break;
              }
              if (node.range[1] === comment.extendedRange[0]) {
                if (!node.trailingComments) {
                  node.trailingComments = [];
                }
                node.trailingComments.push(comment);
                comments.splice(cursor, 1);
              } else {
                cursor += 1;
              }
            }
            if (cursor === comments.length) {
              return VisitorOption.Break;
            }
            if (comments[cursor].extendedRange[0] > node.range[1]) {
              return VisitorOption.Skip;
            }
          }
        });
        return tree;
      }
      exports.version = '1.3.1';
      exports.Syntax = Syntax;
      exports.traverse = traverse;
      exports.replace = replace;
      exports.attachComments = attachComments;
      exports.VisitorKeys = VisitorKeys;
      exports.VisitorOption = VisitorOption;
      exports.Controller = Controller;
    }));
  });
  require('/tools/entry-point.js');
}.call(this, this));
