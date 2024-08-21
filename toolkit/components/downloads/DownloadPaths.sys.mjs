/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides methods for giving names and paths to files being downloaded.
 */

export var DownloadPaths = {
  /**
   * Sanitizes an arbitrary string via mimeSvc.validateFileNameForSaving.
   *
   * If the filename being validated is one that was returned from a
   * file picker, pass false for compressWhitespaces and true for
   * allowInvalidFilenames. Otherwise, the default values of the arguments
   * should generally be used.
   *
   * @param {string} leafName The full leaf name to sanitize
   * @param {boolean} [compressWhitespaces] Whether consecutive whitespaces
   *        should be compressed. The default value is true.
   * @param {boolean} [allowInvalidFilenames] Allow invalid and dangerous
   *        filenames and extensions as is.
   * @param {boolean} [allowDirectoryNames] Allow invalid or dangerous file
   *        names if the name is a valid and safe directory name.
   */
  sanitize(
    leafName,
    {
      compressWhitespaces = true,
      allowInvalidFilenames = false,
      allowDirectoryNames = false,
    } = {}
  ) {
    const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

    let flags = mimeSvc.VALIDATE_SANITIZE_ONLY | mimeSvc.VALIDATE_DONT_TRUNCATE;
    if (!compressWhitespaces) {
      flags |= mimeSvc.VALIDATE_DONT_COLLAPSE_WHITESPACE;
    }
    if (allowInvalidFilenames) {
      flags |= mimeSvc.VALIDATE_ALLOW_INVALID_FILENAMES;
    }
    if (allowDirectoryNames) {
      flags |= mimeSvc.VALIDATE_ALLOW_DIRECTORY_NAMES;
    }
    return mimeSvc.validateFileNameForSaving(leafName, "", flags);
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
