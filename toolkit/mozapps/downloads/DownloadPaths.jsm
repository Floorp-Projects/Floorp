/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
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
 * The Original Code is Download Manager Utility Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var EXPORTED_SYMBOLS = [
  "DownloadPaths",
];

/**
 * This module provides the DownloadPaths object which contains methods for
 * giving names and paths to files being downloaded.
 *
 * List of methods:
 *
 * nsILocalFile
 * createNiceUniqueFile(nsILocalFile aLocalFile)
 *
 * [string base, string ext]
 * splitBaseNameAndExtension(string aLeafName)
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const DownloadPaths = {
  /**
   * Creates a uniquely-named file starting from the name of the provided file.
   * If a file with the provided name already exists, the function attempts to
   * create nice alternatives, like "base(1).ext" (instead of "base-1.ext").
   *
   * If a unique name cannot be found, the function throws the XPCOM exception
   * NS_ERROR_FILE_TOO_BIG. Other exceptions, like NS_ERROR_FILE_ACCESS_DENIED,
   * can also be expected.
   *
   * @param aTemplateFile
   *        nsILocalFile whose leaf name is going to be used as a template. The
   *        provided object is not modified.
   * @returns A new instance of an nsILocalFile object pointing to the newly
   *          created empty file. On platforms that support permission bits, the
   *          file is created with permissions 600.
   */
  createNiceUniqueFile: function DP_createNiceUniqueFile(aTemplateFile) {
    // Work on a clone of the provided template file object.
    var curFile = aTemplateFile.clone().QueryInterface(Ci.nsILocalFile);
    var [base, ext] = DownloadPaths.splitBaseNameAndExtension(curFile.leafName);
    // Try other file names, for example "base(1).txt" or "base(1).tar.gz",
    // only if the file name initially set already exists.
    for (let i = 1; i < 10000 && curFile.exists(); i++) {
      curFile.leafName = base + "(" + i + ")" + ext;
    }
    // At this point we hand off control to createUnique, which will create the
    // file with the name we chose, if it is valid. If not, createUnique will
    // attempt to modify it again, for example it will shorten very long names
    // that can't be created on some platforms, and for which a normal call to
    // nsIFile.create would result in NS_ERROR_FILE_NOT_FOUND. This can result
    // very rarely in strange names like "base(9999).tar-1.gz" or "ba-1.gz".
    curFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
    return curFile;
  },

  /**
   * Separates the base name from the extension in a file name, recognizing some
   *  double extensions like ".tar.gz".
   *
   * @param aLeafName
   *        The full leaf name to be parsed. Be careful when processing names
   *        containing leading or trailing dots or spaces.
   * @returns [base, ext]
   *          The base name of the file, which can be empty, and its extension,
   *          which always includes the leading dot unless it's an empty string.
   *          Concatenating the two items always results in the original name.
   */
  splitBaseNameAndExtension: function DP_splitBaseNameAndExtension(aLeafName) {
    // The following regular expression is built from these key parts:
    //  .*?                      Matches the base name non-greedily.
    //  \.[A-Z0-9]{1,3}          Up to three letters or numbers preceding a
    //                           double extension.
    //  \.(?:gz|bz2|Z)           The second part of common double extensions.
    //  \.[^.]*                  Matches any extension or a single trailing dot.
    var [, base, ext] = /(.*?)(\.[A-Z0-9]{1,3}\.(?:gz|bz2|Z)|\.[^.]*)?$/i
                        .exec(aLeafName);
    // Return an empty string instead of undefined if no extension is found.
    return [base, ext || ""];
  }
};
