/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides methods for giving names and paths to files being downloaded.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadPaths"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

/**
 * Platform-dependent regular expression used by the "sanitize" method.
 */
XPCOMUtils.defineLazyGetter(this, "gConvertToSpaceRegExp", () => {
  // Note: we remove colons everywhere to avoid issues in subresource URL
  // parsing, as well as filename restrictions on some OSes (see bug 1562176).
  /* eslint-disable no-control-regex */
  switch (AppConstants.platform) {
    // On mobile devices, the file system may be very limited in what it
    // considers valid characters. To avoid errors, sanitize conservatively.
    case "android":
      return /[\x00-\x1f\x7f-\x9f:*?|"<>;,+=\[\]]+/g;
    case "win":
      return /[\x00-\x1f\x7f-\x9f:*?|]+/g;
    default:
      return /[\x00-\x1f\x7f-\x9f:]+/g;
  }
  /* eslint-enable no-control-regex */
});

var DownloadPaths = {
  /**
   * Sanitizes an arbitrary string for use as the local file name of a download.
   * The input is often a document title or a manually edited name. The output
   * can be an empty string if the input does not include any valid character.
   *
   * The length of the resulting string is not limited, because restrictions
   * apply to the full path name after the target folder has been added.
   *
   * Splitting the base name and extension to add a counter or to identify the
   * file type should only be done after the sanitization process, because it
   * can alter the final part of the string or remove leading dots.
   *
   * Runs of slashes and backslashes are replaced with an underscore.
   *
   * On Windows, the angular brackets `<` and `>` are replaced with parentheses,
   * and double quotes are replaced with single quotes.
   *
   * Runs of control characters are replaced with a space. On Mac, colons are
   * also included in this group. On Windows, stars, question marks, and pipes
   * are additionally included. On Android, semicolons, commas, plus signs,
   * equal signs, and brackets are additionally included.
   *
   * Leading and trailing dots and whitespace are removed on all platforms. This
   * avoids the accidental creation of hidden files on Unix and invalid or
   * inaccessible file names on Windows. These characters are not removed when
   * located at the end of the base name or at the beginning of the extension.
   *
   * @param {string} leafName The full leaf name to sanitize
   * @param {boolean} [compressWhitespaces] Whether consecutive whitespaces
   *        should be compressed.
   */
  sanitize(leafName, { compressWhitespaces = true } = {}) {
    if (AppConstants.platform == "win") {
      leafName = leafName
        .replace(/</g, "(")
        .replace(/>/g, ")")
        .replace(/"/g, "'");
    }
    leafName = leafName
      .replace(/[\\/]+/g, "_")
      .replace(/[\u200e\u200f\u202a-\u202e]/g, "")
      .replace(gConvertToSpaceRegExp, " ");
    if (compressWhitespaces) {
      leafName = leafName.replace(/\s{2,}/g, " ");
    }
    return leafName.replace(/^[\s\u180e.]+|[\s\u180e.]+$/g, "");
  },

  /**
   * Creates a uniquely-named file starting from the name of the provided file.
   * If a file with the provided name already exists, the function attempts to
   * create nice alternatives, like "base(1).ext" (instead of "base-1.ext").
   *
   * If a unique name cannot be found, the function throws the XPCOM exception
   * NS_ERROR_FILE_TOO_BIG. Other exceptions, like NS_ERROR_FILE_ACCESS_DENIED,
   * can also be expected.
   *
   * @param templateFile
   *        nsIFile whose leaf name is going to be used as a template. The
   *        provided object is not modified.
   *
   * @return A new instance of an nsIFile object pointing to the newly created
   *         empty file. On platforms that support permission bits, the file is
   *         created with permissions 644.
   */
  createNiceUniqueFile(templateFile) {
    // Work on a clone of the provided template file object.
    let curFile = templateFile.clone().QueryInterface(Ci.nsIFile);
    let [base, ext] = DownloadPaths.splitBaseNameAndExtension(curFile.leafName);
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
    curFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o644);
    return curFile;
  },

  /**
   * Separates the base name from the extension in a file name, recognizing some
   * double extensions like ".tar.gz".
   *
   * @param leafName
   *        The full leaf name to be parsed. Be careful when processing names
   *        containing leading or trailing dots or spaces.
   *
   * @return [base, ext]
   *         The base name of the file, which can be empty, and its extension,
   *         which always includes the leading dot unless it's an empty string.
   *         Concatenating the two items always results in the original name.
   */
  splitBaseNameAndExtension(leafName) {
    // The following regular expression is built from these key parts:
    //  .*?                      Matches the base name non-greedily.
    //  \.[A-Z0-9]{1,3}          Up to three letters or numbers preceding a
    //                           double extension.
    //  \.(?:gz|bz2|Z)           The second part of common double extensions.
    //  \.[^.]*                  Matches any extension or a single trailing dot.
    let [, base, ext] = /(.*?)(\.[A-Z0-9]{1,3}\.(?:gz|bz2|Z)|\.[^.]*)?$/i.exec(
      leafName
    );
    // Return an empty string instead of undefined if no extension is found.
    return [base, ext || ""];
  },
};
