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

var fs = require('fs');
var path = require('path');

var promise = require('./promise');
var util = require('./util');

/**
 * A set of functions defining a filesystem API for fileparser.js
 */

exports.join = path.join.bind(path);
exports.dirname = path.dirname.bind(path);
exports.sep = path.sep;
exports.home = process.env.HOME;

/**
 * The NodeJS docs suggest using ``pathname.split(path.sep)`` to cut a path
 * into a number of components. But this doesn't take into account things like
 * path normalization and removing the initial (or trailing) blanks from
 * absolute (or / terminated) paths.
 * http://www.nodejs.org/api/path.html#path_path_sep
 * @param pathname (string) The part to cut up
 * @return An array of path components
 */
exports.split = function(pathname) {
  pathname = path.normalize(pathname);
  var parts = pathname.split(exports.sep);
  return parts.filter(function(part) {
    return part !== '';
  });
};

/**
 * @param pathname string, path of an existing directory
 * @param matches optional regular expression - filter output to include only
 * the files that match the regular expression. The regexp is applied to the
 * filename only not to the full path
 * @return A promise of an array of stat objects for each member of the
 * directory pointed to by ``pathname``, each containing 2 extra properties:
 * - pathname: The full pathname of the file
 * - filename: The final filename part of the pathname
 */
exports.ls = function(pathname, matches) {
  var deferred = promise.defer();

  fs.readdir(pathname, function(err, files) {
    if (err) {
      deferred.reject(err);
    }
    else {
      if (matches) {
        files = files.filter(function(file) {
          return matches.test(file);
        });
      }

      var statsPromise = util.promiseEach(files, function(filename) {
        var filepath = path.join(pathname, filename);
        return exports.stat(filepath).then(function(stats) {
          stats.filename = filename;
          stats.pathname = filepath;
          return stats;
        });
      });

      statsPromise.then(deferred.resolve, deferred.reject);
    }
  });

  return deferred.promise;
};

/**
 * stat() is annoying because it considers stat('/doesnt/exist') to be an
 * error, when the point of stat() is to *find* *out*. So this wrapper just
 * converts 'ENOENT' i.e. doesn't exist to { exists:false } and adds
 * exists:true to stat blocks from existing paths
 */
exports.stat = function(pathname) {
  var deferred = promise.defer();

  fs.stat(pathname, function(err, stats) {
    if (err) {
      if (err.code === 'ENOENT') {
        deferred.resolve({
          exists: false,
          isDir: false,
          isFile: false
        });
      }
      else {
        deferred.reject(err);
      }
    }
    else {
      deferred.resolve({
        exists: true,
        isDir: stats.isDirectory(),
        isFile: stats.isFile()
      });
    }
  });

  return deferred.promise;
};

/**
 * We may read the first line of a file to describe it?
 * Right now, however, we do nothing.
 */
exports.describe = function(pathname) {
  return promise.resolve('');
};
