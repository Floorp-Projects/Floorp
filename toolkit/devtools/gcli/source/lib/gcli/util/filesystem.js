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

var Cu = require('chrome').Cu;
var Cc = require('chrome').Cc;
var Ci = require('chrome').Ci;

var OS = Cu.import('resource://gre/modules/osfile.jsm', {}).OS;

/**
 * A set of functions that don't really belong in 'fs' (because they're not
 * really universal in scope) but also kind of do (because they're not specific
 * to GCLI
 */

exports.join = OS.Path.join;
exports.sep = OS.Path.sep;
exports.dirname = OS.Path.dirname;

// On B2G, there is no home folder
var home = null;
try {
  var dirService = Cc['@mozilla.org/file/directory_service;1']
                     .getService(Ci.nsIProperties);
  home = dirService.get('Home', Ci.nsIFile).path;
} catch(e) {}
exports.home = home;

if ('winGetDrive' in OS.Path) {
  exports.sep = '\\';
}
else {
  exports.sep = '/';
}

/**
 * Split a path into its components.
 * @param pathname (string) The part to cut up
 * @return An array of path components
 */
exports.split = function(pathname) {
  return OS.Path.split(pathname).components;
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
  var iterator = new OS.File.DirectoryIterator(pathname);
  var entries = [];

  var iteratePromise = iterator.forEach(function(entry) {
    entries.push({
      exists: true,
      isDir: entry.isDir,
      isFile: !entry.isFile,
      filename: entry.name,
      pathname: entry.path
    });
  });

  return iteratePromise.then(function onSuccess() {
      iterator.close();
      return entries;
    },
    function onFailure(reason) {
      iterator.close();
      throw reason;
    }
  );
};

/**
 * stat() is annoying because it considers stat('/doesnt/exist') to be an
 * error, when the point of stat() is to *find* *out*. So this wrapper just
 * converts 'ENOENT' i.e. doesn't exist to { exists:false } and adds
 * exists:true to stat blocks from existing paths
 */
exports.stat = function(pathname) {
  var onResolve = function(stats) {
    return {
      exists: true,
      isDir: stats.isDir,
      isFile: !stats.isFile
    };
  };

  var onReject = function(err) {
    if (err instanceof OS.File.Error && err.becauseNoSuchFile) {
      return {
        exists: false,
        isDir: false,
        isFile: false
      };
    }
    throw err;
  };

  return OS.File.stat(pathname).then(onResolve, onReject);
};

/**
 * We may read the first line of a file to describe it?
 * Right now, however, we do nothing.
 */
exports.describe = function(pathname) {
  return Promise.resolve('');
};
