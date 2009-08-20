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
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ["PlacesItem", "Bookmark", "BookmarkFolder",
  "BookmarkMicsum", "BookmarkQuery", "Livemark", "BookmarkSeparator"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");

function PlacesItem(uri) {
  this._PlacesItem_init(uri);
}
PlacesItem.prototype = {
  decrypt: function PlacesItem_decrypt(passphrase) {
    // Do the normal CryptoWrapper decrypt, but change types before returning
    let clear = CryptoWrapper.prototype.decrypt.apply(this, arguments);

    // Convert the abstract places item to the actual object type
    if (!this.deleted)
      this.__proto__ = this.getTypeObject(this.type).prototype;

    return clear;
  },

  getTypeObject: function PlacesItem_getTypeObject(type) {
    switch (type) {
      case "bookmark":
        return Bookmark;
      case "microsummary":
        return BookmarkMicsum;
      case "query":
        return BookmarkQuery;
      case "folder":
        return BookmarkFolder;
      case "livemark":
        return Livemark;
      case "separator":
        return BookmarkSeparator;
      case "item":
        return PlacesItem;
    }
    throw "Unknown places item object type: " + type;
  },

  __proto__: CryptoWrapper.prototype,
  _logName: "Record.PlacesItem",

  _PlacesItem_init: function BmkItemRec_init(uri) {
    this._CryptoWrap_init(uri);
    this.cleartext = {};
    this.type = "item";
  },
};

Utils.deferGetSet(PlacesItem, "cleartext", ["predecessorid", "type"]);

function Bookmark(uri) {
  this._Bookmark_init(uri);
}
Bookmark.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Bookmark",

  _Bookmark_init: function BmkRec_init(uri) {
    this._PlacesItem_init(uri);
    this.type = "bookmark";
  },
};

Utils.deferGetSet(Bookmark, "cleartext", ["title", "bmkUri", "description",
  "loadInSidebar", "tags", "keyword"]);

function BookmarkMicsum(uri) {
  this._BookmarkMicsum_init(uri);
}
BookmarkMicsum.prototype = {
  __proto__: Bookmark.prototype,
  _logName: "Record.BookmarkMicsum",

  _BookmarkMicsum_init: function BmkMicsumRec_init(uri) {
    this._Bookmark_init(uri);
    this.type = "microsummary";
  },
};

Utils.deferGetSet(BookmarkMicsum, "cleartext", ["generatorUri", "staticTitle"]);

function BookmarkQuery(uri) {
  this._BookmarkQuery_init(uri);
}
BookmarkQuery.prototype = {
  __proto__: Bookmark.prototype,
  _logName: "Record.BookmarkQuery",

  _BookmarkQuery_init: function BookmarkQuery_init(uri) {
    this._Bookmark_init(uri);
    this.type = "query";
  },
};

Utils.deferGetSet(BookmarkQuery, "cleartext", ["folderName"]);

function BookmarkFolder(uri) {
  this._BookmarkFolder_init(uri);
}
BookmarkFolder.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Folder",

  _BookmarkFolder_init: function FolderRec_init(uri) {
    this._PlacesItem_init(uri);
    this.type = "folder";
  },
};

Utils.deferGetSet(BookmarkFolder, "cleartext", "title");

function Livemark(uri) {
  this._Livemark_init(uri);
}
Livemark.prototype = {
  __proto__: BookmarkFolder.prototype,
  _logName: "Record.Livemark",

  _Livemark_init: function LvmkRec_init(uri) {
    this._BookmarkFolder_init(uri);
    this.type = "livemark";
  },
};

Utils.deferGetSet(Livemark, "cleartext", ["siteUri", "feedUri"]);

function BookmarkSeparator(uri) {
  this._BookmarkSeparator_init(uri);
}
BookmarkSeparator.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Separator",

  _BookmarkSeparator_init: function SepRec_init(uri) {
    this._PlacesItem_init(uri);
    this.type = "separator";
  }
};
