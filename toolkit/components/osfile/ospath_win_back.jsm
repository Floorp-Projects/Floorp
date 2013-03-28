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
 *
 * Additionally, |normalize| can convert a path containing slash-
 * separated components to a path containing backslash-separated
 * components.
 */
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = ["OS"];
  Components.utils.import("resource://gre/modules/osfile/osfile_win_allthreads.jsm", this);
}
(function(exports) {
   "use strict";
   if (!exports.OS) {
     exports.OS = {};
   }
   if (!exports.OS.Win) {
     exports.OS.Win = {};
   }
   if (exports.OS.Win.Path) {
     return; // Avoid double-initialization
   }
   exports.OS.Win.Path = {
     /**
      * Return the final part of the path.
      * The final part of the path is everything after the last "\\".
      */
     basename: function basename(path) {
       ensureNotUNC(path);
       return path.slice(Math.max(path.lastIndexOf("\\"),
         path.lastIndexOf(":")) + 1);
     },

     /**
      * Return the directory part of the path.  The directory part
      * of the path is everything before the last "\\", including
      * the drive/server name.
      *
      * If the path contains no directory, return the drive letter,
      * or "." if the path contains no drive letter or if option
      * |winNoDrive| is set.
      *
      * @param {string} path The path.
      * @param {*=} options Platform-specific options controlling the behavior
      * of this function. This implementation supports the following options:
      *  - |winNoDrive| If |true|, also remove the letter from the path name.
      */
     dirname: function dirname(path, options) {
       ensureNotUNC(path);
       let noDrive = (options && options.winNoDrive);

       // Find the last occurrence of "\\"
       let index = path.lastIndexOf("\\");
       if (index == -1) {
         // If there is no directory component...
         if (!noDrive) {
           // Return the drive path if possible, falling back to "."
           return this.winGetDrive(path) || ".";
         } else {
           // Or just "."
           return ".";
         }
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
      * Under Windows, this will return "$TMP\foo\bar".
      */
     join: function join(path /*...*/) {
       let paths = [];
       let root;
       let absolute = false;
       for each(let subpath in arguments) {
         let drive = this.winGetDrive(subpath);
         let abs   = this.winIsAbsolute(subpath);
         if (drive) {
           root = drive;
           paths = [trimBackslashes(subpath.slice(drive.length))];
           absolute = abs;
         } else if (abs) {
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
     },

     /**
      * Return the drive letter of a path, or |null| if the
      * path does not contain a drive letter.
      */
     winGetDrive: function winGetDrive(path) {
       ensureNotUNC(path);
       let index = path.indexOf(":");
       if (index <= 0) return null;
       return path.slice(0, index + 1);
     },

     /**
      * Return |true| if the path is absolute, |false| otherwise.
      *
      * We consider that a path is absolute if it starts with "\\"
      * or "driveletter:\\".
      */
     winIsAbsolute: function winIsAbsolute(path) {
       ensureNotUNC(path);
       return this._winIsAbsolute(path);
     },
     /**
      * As |winIsAbsolute|, but does not check for UNC.
      * Useful mostly as an internal utility, for normalization.
      */
     _winIsAbsolute: function _winIsAbsolute(path) {
       let index = path.indexOf(":");
       return path.length > index + 1 && path[index + 1] == "\\";
     },

     /**
      * Normalize a path by removing any unneeded ".", "..", "\\".
      * Also convert any "/" to a "\\".
      */
     normalize: function normalize(path) {
       let stack = [];

       // Remove the drive (we will put it back at the end)
       let root = this.winGetDrive(path);
       if (root) {
         path = path.slice(root.length);
       }

       // Remember whether we need to restore a leading "\\".
       let absolute = this._winIsAbsolute(path);

       // Normalize "/" to "\\"
       path = path.replace("/", "\\");

       // And now, fill |stack| from the components,
       // popping whenever there is a ".."
       path.split("\\").forEach(function loop(v) {
         switch (v) {
         case "":  case ".": // Ignore
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

       // Put everything back together
       let result = stack.join("\\");
       if (absolute) {
         result = "\\" + result;
       }
       if (root) {
         result = root + result;
       }
       return result;
     },

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
     split: function split(path) {
       return {
         absolute: this.winIsAbsolute(path),
         winDrive: this.winGetDrive(path),
         components: path.split("\\")
       };
     }
   };

    /**
     * Utility function: check that a path is not a UNC path,
     * as the current implementation of |Path| does not support
     * UNC grammars.
     *
     * We consider that any path starting with "\\\\" is a UNC path.
     */
    let ensureNotUNC = function ensureNotUNC(path) {
       if (!path) {
          throw new TypeError("Expecting a non-null path");
       }
       if (path.length >= 2 && path[0] == "\\" && path[1] == "\\") {
          throw new Error("Module Path cannot handle UNC-formatted paths yet: " + path);
       }
    };

    /**
     * Utility function: Remove any leading/trailing backslashes
     * from a string.
     */
    let trimBackslashes = function trimBackslashes(string) {
      return string.replace(/^\\+|\\+$/g,'');
    };

   exports.OS.Path = exports.OS.Win.Path;
}(this));
