/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
#  Philipp von Weitershausen <philipp@weitershausen.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/
EXPORTED_SYMBOLS = [ "FileUtils" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

XPCOMUtils.defineLazyServiceGetter(this, "gDirService",
                                   "@mozilla.org/file/directory_service;1",
                                   "nsIProperties");

var FileUtils = {
  MODE_RDONLY   : 0x01,
  MODE_WRONLY   : 0x02,
  MODE_CREATE   : 0x08,
  MODE_APPEND   : 0x10,
  MODE_TRUNCATE : 0x20,

  PERMS_FILE      : 0644,
  PERMS_DIRECTORY : 0755,

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
   *          the way are.
   */
  getFile: function FileUtils_getFile(key, pathArray, followLinks) {
    var file = this.getDir(key, pathArray.slice(0, -1), true, followLinks);
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
   * @param   followLinks (optional)
   *          true if links should be followed, false otherwise.
   * @return  nsIFile object for the location specified.
   */
  getDir: function FileUtils_getDir(key, pathArray, shouldCreate, followLinks) {
    var dir = gDirService.get(key, Ci.nsILocalFile);
    for (var i = 0; i < pathArray.length; ++i) {
      dir.append(pathArray[i]);
      if (shouldCreate && !dir.exists())
        dir.create(Ci.nsILocalFile.DIRECTORY_TYPE, this.PERMS_DIRECTORY);
    }
    if (!followLinks)
      dir.followLinks = false;
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
  openFileOutputStream: function FileUtils_openFileOutputStream(file, modeFlags) {
    var fos = Cc["@mozilla.org/network/file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);
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
  openSafeFileOutputStream: function FileUtils_openSafeFileOutputStream(file, modeFlags) {
    var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);
    return this._initFileOutputStream(fos, file, modeFlags);
  },

 _initFileOutputStream: function FileUtils__initFileOutputStream(fos, file, modeFlags) {
    if (modeFlags === undefined)
      modeFlags = this.MODE_WRONLY | this.MODE_CREATE | this.MODE_TRUNCATE;
    fos.init(file, modeFlags, this.PERMS_FILE, fos.DEFER_OPEN);
    return fos;
  },

  /**
   * Closes a safe file output stream.
   * @param   stream
   *          The stream to close.
   */
  closeSafeFileOutputStream: function FileUtils_closeSafeFileOutputStream(stream) {
    if (stream instanceof Ci.nsISafeOutputStream) {
      try {
        stream.finish();
        return;
      }
      catch (e) {
      }
    }
    stream.close();
  },

  File: Components.Constructor("@mozilla.org/file/local;1", Ci.nsILocalFile, "initWithPath")
};
