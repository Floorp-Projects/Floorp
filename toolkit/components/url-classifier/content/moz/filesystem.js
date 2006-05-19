/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Boodman <aa@google.com> (original author)
 *   Raphael Moll <raphael@google.com>
 *   Fritz Schneider <fritz@google.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// Utilities for working with nsIFile and related interfaces.

/**
 * Stub for an nsIFile wrapper which doesn't exist yet. Perhaps in the future
 * we could add functionality to nsILocalFile which would be useful to us here,
 * but for now, no need for such. This could be done by setting 
 * __proto__ to an instance of nsIFile, for example. Neat.
 */
var G_File = {};

/**
 * Returns an nsIFile pointing to the user's home directory, or optionally, a
 * file inside that dir.
 */
G_File.getHomeFile = function(opt_file) {
  return this.getSpecialFile("Home", opt_file);
}

/**
 * Returns an nsIFile pointing to the current profile folder, or optionally, a
 * file inside that dir.
 */
G_File.getProfileFile = function(opt_file) {
  return this.getSpecialFile("ProfD", opt_file);
}

/**
 * returns an nsIFile pointing to the temporary dir, or optionally, a file 
 * inside that dir.
 */
G_File.getTempFile = function(opt_file) {
  return this.getSpecialFile("TmpD", opt_file);
}

/**
 * Returns an nsIFile pointing to one of the special named directories defined 
 * by Firefox, such as the user's home directory, the profile directory, etc. 
 * 
 * As a convenience, callers may specify the opt_file argument to get that file
 * within the special directory instead.
 *
 * http://lxr.mozilla.org/seamonkey/source/xpcom/io/nsDirectoryServiceDefs.h
 * http://kb.mozillazine.org/File_IO#Getting_special_files
 */
G_File.getSpecialFile = function(loc, opt_file) {
  var file = Cc["@mozilla.org/file/directory_service;1"]
             .getService(Ci.nsIProperties)
             .get(loc, Ci.nsILocalFile);

  if (opt_file) {
    file.append(opt_file);
  }

  return file;
}

/**
 * Creates and returns a pointer to a unique file in the temporary directory
 * with an optional base name.
 */
G_File.createUniqueTempFile = function(opt_baseName) {
  var baseName = (opt_baseName || (new Date().getTime())) + ".tmp";

  var file = this.getSpecialFile("TmpD", baseName);
  file.createUnique(file.NORMAL_FILE_TYPE, 0644);

  return file;
}

/**
 * Creates and returns a pointer to a unique temporary directory, with
 * an optional base name.
 */
G_File.createUniqueTempDir = function(opt_baseName) {
  var baseName = (opt_baseName || (new Date().getTime())) + ".tmp";

  var dir = this.getSpecialFile("TmpD", baseName);
  dir.createUnique(dir.DIRECTORY_TYPE, 0744);

  return dir;
}

/**
 * Static method to retrieve an nsIFile from a file:// URI.
 */
G_File.fromFileURI = function(uri) {
  // Ensure they use file:// url's: discourages platform-specific paths
  if (uri.indexOf("file://") != 0)
    throw new Error("File path must be a file:// URL");

  var fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"]
                    .getService(Ci.nsIFileProtocolHandler);
  return fileHandler.getFileFromURLSpec(uri);
}

// IO Constants

G_File.PR_RDONLY = 0x01;      // read-only
G_File.PR_WRONLY = 0x02;      // write only
G_File.PR_RDWR = 0x04;        // reading and writing
G_File.PR_CREATE_FILE = 0x08; // create if it doesn't exist
G_File.PR_APPEND = 0x10;      // file pntr reset to end prior to writes
G_File.PR_TRUNCATE = 0x20;    // file exists its length is set to zero
G_File.PR_SYNC = 0x40;        // writes wait for data to be physically written
G_File.PR_EXCL = 0x80;        // file does not exist ? created : no action

// The character(s) to use for line-endings, which are platform-specific.
// This doesn't work for mac os9, but I don't know of a good way to detect
// OS9-ness from JS.
G_File.__defineGetter__("LINE_END_CHAR", function() {
  var end_char = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULRuntime)
                 .OS == "WINNT" ? "\r\n" : "\n";

  // Cache result
  G_File.__defineGetter__("LINE_END_CHAR", function() { return end_char; });
  return end_char;
});

/**
 * A class which can read a file incrementally or all at once. Parameter can be
 * either an nsIFile instance or a string file:// URI.
 * Note that this class is not compatible with non-ascii data.
 */
function G_FileReader(file) {
  this.file_ = isString(file) ? G_File.fromFileURI(file) : file;
}

/**
 * Utility method to read the entire contents of a file. Parameter can be either
 * an nsIFile instance or a string file:// URI.
 */
G_FileReader.readAll = function(file) { 
  var reader = new G_FileReader(file);

  try {
    return reader.read();
  } finally {
    reader.close();
  }
}

/**
 * Read up to opt_maxBytes from the stream. If opt_maxBytes is not specified, 
 * the entire file is read.
 */
G_FileReader.prototype.read = function(opt_maxBytes) {
  if (!this.stream_) {
    var fs = Cc["@mozilla.org/network/file-input-stream;1"]
               .createInstance(Ci.nsIFileInputStream);
    fs.init(this.file_, G_File.PR_RDONLY, 0444, 0);

    this.stream_ = Cc["@mozilla.org/scriptableinputstream;1"]
                     .createInstance(Ci.nsIScriptableInputStream);
    this.stream_.init(fs);
  }
  
  if (!isDef(opt_maxBytes)) {
    opt_maxBytes = this.stream_.available();
  }

  return this.stream_.read(opt_maxBytes);
}

/**
 * Close the stream. This step is required when reading is done.
 */
G_FileReader.prototype.close = function(opt_maxBytes) {
  if (this.stream_) {
    this.stream_.close();
    this.stream_ = null;
  }
}


// TODO(anyone): Implement G_LineReader. The interface should be something like:
// for (var line = null; line = reader.readLine();) {
//   // do something with line
// }


/**
 * Writes a file incrementally or all at once.
 * Note that this class is not compatible with non-ascii data.
 */
function G_FileWriter(file, opt_append) {
  this.file_ = typeof file == "string" ? G_File.fromFileURI(file) : file;
  this.append_ = !!opt_append;
}

/**
 * Helper to write to a file in one step.
 */
G_FileWriter.writeAll = function(file, data, opt_append) { 
  var writer = new G_FileWriter(file, opt_append);

  try {
    return writer.write(data);
  } finally {
    writer.close();
    return 0;
  }
}

/**
 * Write bytes out to the file. Returns the number of bytes written.
 */
G_FileWriter.prototype.write = function(data) {
  if (!this.stream_) {
    this.stream_ = Cc["@mozilla.org/network/file-output-stream;1"]
                     .createInstance(Ci.nsIFileOutputStream);

    var flags = G_File.PR_WRONLY | 
                G_File.PR_CREATE_FILE | 
                (this.append_ ? G_File.PR_APPEND : G_File.PR_TRUNCATE);


    this.stream_.init(this.file_, 
                      flags, 
                      -1 /* default perms */, 
                      0 /* no special behavior */);
  }

  return this.stream_.write(data, data.length);
}

/**
 * Writes bytes out to file followed by the approriate line-ending character for
 * the current platform.
 */
G_FileWriter.prototype.writeLine = function(data) {
  this.write(data + G_File.LINE_END_CHAR);
}


/**
 * Closes the file. This must becalled when writing is done.
 */
G_FileWriter.prototype.close = function() {
  if (this.stream_) {
    this.stream_.close();
    this.stream_ = null;
  }
}

#ifdef DEBUG
function TEST_G_File() {
  if (G_GDEBUG) {
    var z = "gfile UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    // Test G_File.createUniqueTempir
    try {
      var dir = G_File.createUniqueTempDir();
    } catch(e) {
      G_Debug(z, e);
      G_Assert(z, false, "Failed to create temp dir");
    }
    
    G_Assert(z, dir.exists(), "Temp dir doesn't exist: " + dir.path);
    G_Assert(z, dir.isDirectory(), "Temp dir isn't a directory: " + dir.path);
    G_Assert(z, dir.isReadable(), "Temp dir isn't readable: " + dir.path);
    G_Assert(z, dir.isWritable(), "Temp dir isn't writable: " + dir.path);

    dir.remove(true /* recurse */);

    // TODO: WE NEED MORE UNITTESTS!

    G_Debug(z, "PASS");
  }
}
#endif
