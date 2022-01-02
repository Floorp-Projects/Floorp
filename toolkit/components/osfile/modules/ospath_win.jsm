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
 *
 * Limitations of this implementation.
 *
 * Windows supports 6 distinct grammars for paths. For the moment, this
 * implementation supports the following subset:
 *
 * - drivename:backslash-separated components
 * - backslash-separated components
 * - \\drivename\ followed by backslash-separated components
 *
 * Additionally, |normalize| can convert a path containing slash-
 * separated components to a path containing backslash-separated
 * components.
 */

"use strict";

// Boilerplate used to be able to import this module both from the main
// thread and from worker threads.
if (typeof Components != "undefined") {
  // Global definition of |exports|, to keep everybody happy.
  // In non-main thread, |exports| is provided by the module
  // loader.
  this.exports = {};
} else if (typeof module == "undefined" || typeof exports == "undefined") {
  throw new Error("Please load this module using require()");
}

var EXPORTED_SYMBOLS = [
  "basename",
  "dirname",
  "join",
  "normalize",
  "split",
  "winGetDrive",
  "winIsAbsolute",
  "toFileURI",
  "fromFileURI",
];

/**
 * Return the final part of the path.
 * The final part of the path is everything after the last "\\".
 */
var basename = function(path) {
  if (path.startsWith("\\\\")) {
    // UNC-style path
    let index = path.lastIndexOf("\\");
    if (index != 1) {
      return path.slice(index + 1);
    }
    return ""; // Degenerate case
  }
  return path.slice(
    Math.max(path.lastIndexOf("\\"), path.lastIndexOf(":")) + 1
  );
};
exports.basename = basename;

/**
 * Return the directory part of the path.
 *
 * If the path contains no directory, return the drive letter,
 * or "." if the path contains no drive letter or if option
 * |winNoDrive| is set.
 *
 * Otherwise, return everything before the last backslash,
 * including the drive/server name.
 *
 *
 * @param {string} path The path.
 * @param {*=} options Platform-specific options controlling the behavior
 * of this function. This implementation supports the following options:
 *  - |winNoDrive| If |true|, also remove the letter from the path name.
 */
var dirname = function(path, options) {
  let noDrive = options && options.winNoDrive;

  // Find the last occurrence of "\\"
  let index = path.lastIndexOf("\\");
  if (index == -1) {
    // If there is no directory component...
    if (!noDrive) {
      // Return the drive path if possible, falling back to "."
      return this.winGetDrive(path) || ".";
    }
    // Or just "."
    return ".";
  }

  if (index == 1 && path.charAt(0) == "\\") {
    // The path is reduced to a UNC drive
    if (noDrive) {
      return ".";
    }
    return path;
  }

  // Ignore any occurrence of "\\: immediately before that one
  while (index >= 0 && path[index] == "\\") {
    --index;
  }

  // Compute what is left, removing the drive name if necessary
  let start;
  if (noDrive) {
    start = (this.winGetDrive(path) || "").length;
  } else {
    start = 0;
  }
  return path.slice(start, index + 1);
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
 * Under Windows, this will return "$TMP\foo\bar".
 *
 * Empty components are ignored, i.e. `OS.Path.join("foo", "", "bar)` is the
 * same as `OS.Path.join("foo", "bar")`.
 */
var join = function(...path) {
  let paths = [];
  let root;
  let absolute = false;
  for (let subpath of path) {
    if (subpath == null) {
      throw new TypeError("invalid path component");
    }
    if (subpath == "") {
      continue;
    }
    let drive = this.winGetDrive(subpath);
    if (drive) {
      root = drive;
      let component = trimBackslashes(subpath.slice(drive.length));
      if (component) {
        paths = [component];
      } else {
        paths = [];
      }
      absolute = true;
    } else if (this.winIsAbsolute(subpath)) {
      paths = [trimBackslashes(subpath)];
      absolute = true;
    } else {
      paths.push(trimBackslashes(subpath));
    }
  }
  let result = "";
  if (root) {
    result += root;
  }
  if (absolute) {
    result += "\\";
  }
  result += paths.join("\\");
  return result;
};
exports.join = join;

/**
 * Return the drive name of a path, or |null| if the path does
 * not contain a drive name.
 *
 * Drive name appear either as "DriveName:..." (the return drive
 * name includes the ":") or "\\\\DriveName..." (the returned drive name
 * includes "\\\\").
 */
var winGetDrive = function(path) {
  if (path == null) {
    throw new TypeError("path is invalid");
  }

  if (path.startsWith("\\\\")) {
    // UNC path
    if (path.length == 2) {
      return null;
    }
    let index = path.indexOf("\\", 2);
    if (index == -1) {
      return path;
    }
    return path.slice(0, index);
  }
  // Non-UNC path
  let index = path.indexOf(":");
  if (index <= 0) {
    return null;
  }
  return path.slice(0, index + 1);
};
exports.winGetDrive = winGetDrive;

/**
 * Return |true| if the path is absolute, |false| otherwise.
 *
 * We consider that a path is absolute if it starts with "\\"
 * or "driveletter:\\".
 */
var winIsAbsolute = function(path) {
  let index = path.indexOf(":");
  return path.length > index + 1 && path[index + 1] == "\\";
};
exports.winIsAbsolute = winIsAbsolute;

/**
 * Normalize a path by removing any unneeded ".", "..", "\\".
 * Also convert any "/" to a "\\".
 */
var normalize = function(path) {
  let stack = [];

  if (!path.startsWith("\\\\")) {
    // Normalize "/" to "\\"
    path = path.replace(/\//g, "\\");
  }

  // Remove the drive (we will put it back at the end)
  let root = this.winGetDrive(path);
  if (root) {
    path = path.slice(root.length);
  }

  // Remember whether we need to restore a leading "\\" or drive name.
  let absolute = this.winIsAbsolute(path);

  // And now, fill |stack| from the components,
  // popping whenever there is a ".."
  path.split("\\").forEach(function loop(v) {
    switch (v) {
      case "":
      case ".": // Ignore
        break;
      case "..":
        if (!stack.length) {
          if (absolute) {
            throw new Error("Path is ill-formed: attempting to go past root");
          } else {
            stack.push("..");
          }
        } else if (stack[stack.length - 1] == "..") {
          stack.push("..");
        } else {
          stack.pop();
        }
        break;
      default:
        stack.push(v);
    }
  });

  // Put everything back together
  let result = stack.join("\\");
  if (absolute || root) {
    result = "\\" + result;
  }
  if (root) {
    result = root + result;
  }
  return result;
};
exports.normalize = normalize;

/**
 * Return the components of a path.
 * You should generally apply this function to a normalized path.
 *
 * @return {{
 *   {bool} absolute |true| if the path is absolute, |false| otherwise
 *   {array} components the string components of the path
 *   {string?} winDrive the drive or server for this path
 * }}
 *
 * Other implementations may add additional OS-specific informations.
 */
var split = function(path) {
  return {
    absolute: this.winIsAbsolute(path),
    winDrive: this.winGetDrive(path),
    components: path.split("\\"),
  };
};
exports.split = split;

/**
 * Return the file:// URI file path of the given local file path.
 */
// The case of %3b is designed to match Services.io, but fundamentally doesn't matter.
var toFileURIExtraEncodings = { ";": "%3b", "?": "%3F", "#": "%23" };
var toFileURI = function toFileURI(path) {
  // URI-escape forward slashes and convert backward slashes to forward
  path = this.normalize(path).replace(/[\\\/]/g, m =>
    m == "\\" ? "/" : "%2F"
  );
  // Per https://url.spec.whatwg.org we should not encode [] in the path
  let dontNeedEscaping = { "%5B": "[", "%5D": "]" };
  let uri = encodeURI(path).replace(
    /%(5B|5D)/gi,
    match => dontNeedEscaping[match]
  );

  // add a prefix, and encodeURI doesn't escape a few characters that we do
  // want to escape, so fix that up
  let prefix = "file:///";
  uri = prefix + uri.replace(/[;?#]/g, match => toFileURIExtraEncodings[match]);

  // turn e.g., file:///C: into file:///C:/
  if (uri.charAt(uri.length - 1) === ":") {
    uri += "/";
  }

  return uri;
};
exports.toFileURI = toFileURI;

/**
 * Returns the local file path from a given file URI.
 */
var fromFileURI = function fromFileURI(uri) {
  let url = new URL(uri);
  if (url.protocol != "file:") {
    throw new Error("fromFileURI expects a file URI");
  }

  // strip leading slash, since Windows paths don't start with one
  uri = url.pathname.substr(1);

  let path = decodeURI(uri);
  // decode a few characters where URL's parsing is overzealous
  path = path.replace(/%(3b|3f|23)/gi, match => decodeURIComponent(match));
  path = this.normalize(path);

  // this.normalize() does not remove the trailing slash if the path
  // component is a drive letter. eg. 'C:\'' will not get normalized.
  if (path.endsWith(":\\")) {
    path = path.substr(0, path.length - 1);
  }
  return this.normalize(path);
};
exports.fromFileURI = fromFileURI;

/**
 * Utility function: Remove any leading/trailing backslashes
 * from a string.
 */
var trimBackslashes = function trimBackslashes(string) {
  return string.replace(/^\\+|\\+$/g, "");
};

// ////////// Boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
