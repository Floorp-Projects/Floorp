/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handling native paths.
 *
 * This module contains a number of functions destined to simplify
 * working with native paths through a cross-platform API. Functions
 * of this module will only work with the following assumptions:
 *
 * - paths are valid;
 * - paths are defined with one of the grammars that this module can
 *   parse (see later);
 * - all path concatenations go through function |join|.
 */

"use strict";

// Boilerplate used to be able to import this module both from the main
// thread and from worker threads.
if (typeof Components != "undefined") {
  Components.utils.importGlobalProperties(["URL"]);
  // Global definition of |exports|, to keep everybody happy.
  // In non-main thread, |exports| is provided by the module
  // loader.
  this.exports = {};
} else if (typeof "module" == "undefined" || typeof "exports" == "undefined") {
  throw new Error("Please load this module using require()");
}

let EXPORTED_SYMBOLS = [
  "basename",
  "dirname",
  "join",
  "normalize",
  "split",
  "toFileURI",
  "fromFileURI",
];

/**
 * Return the final part of the path.
 * The final part of the path is everything after the last "/".
 */
let basename = function(path) {
  return path.slice(path.lastIndexOf("/") + 1);
};
exports.basename = basename;

/**
 * Return the directory part of the path.
 * The directory part of the path is everything before the last
 * "/". If the last few characters of this part are also "/",
 * they are ignored.
 *
 * If the path contains no directory, return ".".
 */
let dirname = function(path) {
  let index = path.lastIndexOf("/");
  if (index == -1) {
    return ".";
  }
  while (index >= 0 && path[index] == "/") {
    --index;
  }
  return path.slice(0, index + 1);
};
exports.dirname = dirname;

/**
 * Join path components.
 * This is the recommended manner of getting the path of a file/subdirectory
 * in a directory.
 *
 * Example: Obtaining $TMP/foo/bar in an OS-independent manner
 *  var tmpDir = OS.Constants.Path.tmpDir;
 *  var path = OS.Path.join(tmpDir, "foo", "bar");
 *
 * Under Unix, this will return "/tmp/foo/bar".
 *
 * Empty components are ignored, i.e. `OS.Path.join("foo", "", "bar)` is the
 * same as `OS.Path.join("foo", "bar")`.
 */
let join = function(...path) {
  // If there is a path that starts with a "/", eliminate everything before
  let paths = [];
  for (let subpath of path) {
    if (subpath == null) {
      throw new TypeError("invalid path component");
    }
    if (subpath.length == 0) {
      continue;
    } else if (subpath[0] == "/") {
      paths = [subpath];
    } else {
      paths.push(subpath);
    }
  }
  return paths.join("/");
};
exports.join = join;

/**
 * Normalize a path by removing any unneeded ".", "..", "//".
 */
let normalize = function(path) {
  let stack = [];
  let absolute;
  if (path.length >= 0 && path[0] == "/") {
    absolute = true;
  } else {
    absolute = false;
  }
  path.split("/").forEach(function(v) {
    switch (v) {
    case "":  case ".":// fallthrough
      break;
    case "..":
      if (stack.length == 0) {
        if (absolute) {
          throw new Error("Path is ill-formed: attempting to go past root");
        } else {
          stack.push("..");
        }
      } else {
        if (stack[stack.length - 1] == "..") {
          stack.push("..");
        } else {
          stack.pop();
        }
      }
      break;
    default:
      stack.push(v);
    }
  });
  let string = stack.join("/");
  return absolute ? "/" + string : string;
};
exports.normalize = normalize;

/**
 * Return the components of a path.
 * You should generally apply this function to a normalized path.
 *
 * @return {{
 *   {bool} absolute |true| if the path is absolute, |false| otherwise
 *   {array} components the string components of the path
 * }}
 *
 * Other implementations may add additional OS-specific informations.
 */
let split = function(path) {
  return {
    absolute: path.length && path[0] == "/",
    components: path.split("/")
  };
};
exports.split = split;

/**
 * Returns the file:// URI file path of the given local file path.
 */
// The case of %3b is designed to match Services.io, but fundamentally doesn't matter.
let toFileURIExtraEncodings = {';': '%3b', '?': '%3F', "'": '%27', '#': '%23'};
let toFileURI = function toFileURI(path) {
  let uri = encodeURI(this.normalize(path));

  // add a prefix, and encodeURI doesn't escape a few characters that we do
  // want to escape, so fix that up
  let prefix = "file://";
  uri = prefix + uri.replace(/[;?'#]/g, match => toFileURIExtraEncodings[match]);

  return uri;
};
exports.toFileURI = toFileURI;

/**
 * Returns the local file path from a given file URI.
 */
let fromFileURI = function fromFileURI(uri) {
  let url = new URL(uri);
  if (url.protocol != 'file:') {
    throw new Error("fromFileURI expects a file URI");
  }
  let path = this.normalize(decodeURIComponent(url.pathname));
  return path;
};
exports.fromFileURI = fromFileURI;


//////////// Boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
