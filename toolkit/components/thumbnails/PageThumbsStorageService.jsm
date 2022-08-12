/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const THUMBNAIL_DIRECTORY = "thumbnails";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "gCryptoHash", function() {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

XPCOMUtils.defineLazyGetter(lazy, "gUnicodeConverter", function() {
  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  return converter;
});
function PageThumbsStorageService() {}

PageThumbsStorageService.prototype = {
  classID: Components.ID("{97943eec-0e48-49ef-b7b7-cf4aa0109bb6}"),
  QueryInterface: ChromeUtils.generateQI(["nsIPageThumbsStorageService"]),
  // The path for the storage
  _path: null,
  get path() {
    if (!this._path) {
      this._path = PathUtils.join(
        PathUtils.localProfileDir,
        THUMBNAIL_DIRECTORY
      );
    }
    return this._path;
  },

  getLeafNameForURL(aURL) {
    if (typeof aURL != "string") {
      throw new TypeError("Expecting a string");
    }
    let hash = this._calculateMD5Hash(aURL);
    return hash + ".png";
  },

  getFilePathForURL(aURL) {
    return PathUtils.join(this.path, this.getLeafNameForURL(aURL));
  },

  _calculateMD5Hash(aValue) {
    let hash = lazy.gCryptoHash;
    let value = lazy.gUnicodeConverter.convertToByteArray(aValue);

    hash.init(hash.MD5);
    hash.update(value, value.length);
    return this._convertToHexString(hash.finish(false));
  },

  _convertToHexString(aData) {
    let hex = "";
    for (let i = 0; i < aData.length; i++) {
      hex += ("0" + aData.charCodeAt(i).toString(16)).slice(-2);
    }
    return hex;
  },
};

var EXPORTED_SYMBOLS = ["PageThumbsStorageService"];
