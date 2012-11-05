/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
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

this.DownloadPaths = {
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
