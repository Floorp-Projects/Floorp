/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["FileUtils"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Deprecated",
  "resource://gre/modules/Deprecated.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gDirService",
  "@mozilla.org/file/directory_service;1",
  "nsIProperties"
);

var FileUtils = {
  MODE_RDONLY: 0x01,
  MODE_WRONLY: 0x02,
  MODE_RDWR: 0x04,
  MODE_CREATE: 0x08,
  MODE_APPEND: 0x10,
  MODE_TRUNCATE: 0x20,

  PERMS_FILE: 0o644,
  PERMS_DIRECTORY: 0o755,

  /**
   * Gets a file at the specified hierarchy under a nsIDirectoryService key.
   * @param   key
   *          The Directory Service Key to start from
   * @param   pathArray
   *          An array of path components to locate beneath the directory
   *          specified by |key|. The last item in this array must be the
   *          leaf name of a file.
   * @return  nsIFile object for the file specified. The file is NOT created
   *          if it does not exist, however all required directories along
   *          the way are if pathArray has more than one item.
   */
  getFile: function FileUtils_getFile(key, pathArray) {
    var file = this.getDir(key, pathArray.slice(0, -1), pathArray.length > 1);
    file.append(pathArray[pathArray.length - 1]);
    return file;
  },

  /**
   * Gets a directory at the specified hierarchy under a nsIDirectoryService
   * key.
   * @param   key
   *          The Directory Service Key to start from
   * @param   pathArray
   *          An array of path components to locate beneath the directory
   *          specified by |key|
   * @param   shouldCreate
   *          true if the directory hierarchy specified in |pathArray|
   *          should be created if it does not exist, false otherwise.
   * @return  nsIFile object for the location specified.
   */
  getDir: function FileUtils_getDir(key, pathArray, shouldCreate) {
    var dir = gDirService.get(key, Ci.nsIFile);
    for (var i = 0; i < pathArray.length; ++i) {
      dir.append(pathArray[i]);
    }

    if (shouldCreate) {
      if (Services.startup.startingUp || Services.startup.shuttingDown) {
        Deprecated.warning(
          "Calling FileUtils.getDir(..., ..., true) causes main thread I/O and should be avoided especially during startup/shutdown",
          "https://bugzilla.mozilla.org/show_bug.cgi?id=921157"
        );
      }

      try {
        dir.create(Ci.nsIFile.DIRECTORY_TYPE, this.PERMS_DIRECTORY);
      } catch (ex) {
        if (ex.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
          throw ex;
        }
        // Ignore the exception due to a directory that already exists.
      }
    }

    return dir;
  },

  /**
   * Opens a file output stream for writing.
   * @param   file
   *          The file to write to.
   * @param   modeFlags
   *          (optional) File open flags. Can be undefined.
   * @returns nsIFileOutputStream to write to.
   * @note The stream is initialized with the DEFER_OPEN behavior flag.
   *       See nsIFileOutputStream.
   */
  openFileOutputStream: function FileUtils_openFileOutputStream(
    file,
    modeFlags
  ) {
    var fos = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
      Ci.nsIFileOutputStream
    );
    return this._initFileOutputStream(fos, file, modeFlags);
  },

  /**
   * Opens an atomic file output stream for writing.
   * @param   file
   *          The file to write to.
   * @param   modeFlags
   *          (optional) File open flags. Can be undefined.
   * @returns nsIFileOutputStream to write to.
   * @note The stream is initialized with the DEFER_OPEN behavior flag.
   *       See nsIFileOutputStream.
   *       OpeanAtomicFileOutputStream is generally better than openSafeFileOutputStream
   *       baecause flushing is not needed in most of the issues.
   */
  openAtomicFileOutputStream: function FileUtils_openAtomicFileOutputStream(
    file,
    modeFlags
  ) {
    var fos = Cc[
      "@mozilla.org/network/atomic-file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);
    return this._initFileOutputStream(fos, file, modeFlags);
  },

  /**
   * Opens a safe file output stream for writing.
   * @param   file
   *          The file to write to.
   * @param   modeFlags
   *          (optional) File open flags. Can be undefined.
   * @returns nsIFileOutputStream to write to.
   * @note The stream is initialized with the DEFER_OPEN behavior flag.
   *       See nsIFileOutputStream.
   */
  openSafeFileOutputStream: function FileUtils_openSafeFileOutputStream(
    file,
    modeFlags
  ) {
    var fos = Cc[
      "@mozilla.org/network/safe-file-output-stream;1"
    ].createInstance(Ci.nsIFileOutputStream);
    return this._initFileOutputStream(fos, file, modeFlags);
  },

  _initFileOutputStream: function FileUtils__initFileOutputStream(
    fos,
    file,
    modeFlags
  ) {
    if (modeFlags === undefined) {
      modeFlags = this.MODE_WRONLY | this.MODE_CREATE | this.MODE_TRUNCATE;
    }
    fos.init(file, modeFlags, this.PERMS_FILE, fos.DEFER_OPEN);
    return fos;
  },

  /**
   * Closes an atomic file output stream.
   * @param   stream
   *          The stream to close.
   */
  closeAtomicFileOutputStream: function FileUtils_closeAtomicFileOutputStream(
    stream
  ) {
    if (stream instanceof Ci.nsISafeOutputStream) {
      try {
        stream.finish();
        return;
      } catch (e) {}
    }
    stream.close();
  },

  /**
   * Closes a safe file output stream.
   * @param   stream
   *          The stream to close.
   */
  closeSafeFileOutputStream: function FileUtils_closeSafeFileOutputStream(
    stream
  ) {
    if (stream instanceof Ci.nsISafeOutputStream) {
      try {
        stream.finish();
        return;
      } catch (e) {}
    }
    stream.close();
  },

  File: Components.Constructor(
    "@mozilla.org/file/local;1",
    Ci.nsIFile,
    "initWithPath"
  ),
};
