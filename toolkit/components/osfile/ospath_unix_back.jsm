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
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = ["OS"];
  Components.utils.import("resource://gre/modules/osfile/osfile_unix_allthreads.jsm", this);
}
(function(exports) {
   "use strict";
   if (!exports.OS) {
     exports.OS = {};
   }
   if (!exports.OS.Unix) {
     exports.OS.Unix = {};
   }
   if (exports.OS.Unix.Path) {
     return; // Avoid double-initialization
   }
   exports.OS.Unix.Path = {
     /**
      * Return the final part of the path.
      * The final part of the path is everything after the last "/".
      */
     basename: function basename(path) {
       return path.slice(path.lastIndexOf("/") + 1);
     },
     /**
      * Return the directory part of the path.
      * The directory part of the path is everything before the last
      * "/". If the last few characters of this part are also "/",
      * they are ignored.
      *
      * If the path contains no directory, return ".".
      */
     dirname: function dirname(path) {
       let index = path.lastIndexOf("/");
       if (index == -1) {
         return ".";
       }
       while (index >= 0 && path[index] == "/") {
         --index;
       }
       return path.slice(0, index + 1);
     },
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
      */
     join: function join(path /*...*/) {
       // If there is a path that starts with a "/", eliminate everything before
       let paths = [];
       for each(let i in arguments) {
         if (i.length != 0 && i[0] == "/") {
           paths = [i];
         } else {
           paths.push(i);
         }
       }
       return paths.join("/");
     },
     /**
      * Normalize a path by removing any unneeded ".", "..", "//".
      */
     normalize: function normalize(path) {
       let stack = [];
       let absolute;
       if (path.length >= 0 && path[0] == "/") {
         absolute = true;
       } else {
         absolute = false;
       }
       path.split("/").forEach(function loop(v) {
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
     },
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
     split: function split(path) {
       return {
         absolute: path.length && path[0] == "/",
         components: path.split("/")
       };
     }
   };

   exports.OS.Path = exports.OS.Unix.Path;
}(this));
