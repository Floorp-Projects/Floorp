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

const EXPORTED_SYMBOLS = ['PlacesItem', 'Bookmark', 'BookmarkFolder', 'BookmarkMicsum',
                          'Livemark', 'BookmarkSeparator'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");

Function.prototype.async = Async.sugar;

function PlacesItem(uri, authenticator) {
  this._PlacesItem_init(uri, authenticator);
}
PlacesItem.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Record.PlacesItem",

  _PlacesItem_init: function BmkItemRec_init(uri, authenticator) {
    this._CryptoWrap_init(uri, authenticator);
    this.cleartext = {
    };
  },

  get type() this.cleartext.type,
  set type(value) {
    // XXX check type is valid?
    this.cleartext.type = value;
  }
};

function Bookmark(uri, authenticator) {
  this._Bookmark_init(uri, authenticator);
}
Bookmark.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Bookmark",

  _Bookmark_init: function BmkRec_init(uri, authenticator) {
    this._PlacesItem_init(uri, authenticator);
    this.cleartext.type = "bookmark";
  },

  get title() this.cleartext.title,
  set title(value) {
    this.cleartext.title = value;
  },

  get bmkUri() this.cleartext.uri,
  set bmkUri(value) {
    if (typeof(value) == "string")
      this.cleartext.uri = value;
    else
      this.cleartext.uri = value.spec;
  },

  get description() this.cleartext.description,
  set description(value) {
    this.cleartext.description = value;
  },

  get tags() this.cleartext.tags,
  set tags(value) {
    this.cleartext.tags = value;
  },

  get keyword() this.cleartext.keyword,
  set keyword(value) {
    this.cleartext.keyword = value;
  }
};

function BookmarkMicsum(uri, authenticator) {
  this._BookmarkMicsum_init(uri, authenticator);
}
BookmarkMicsum.prototype = {
  __proto__: Bookmark.prototype,
  _logName: "Record.BookmarkMicsum",

  _BookmarkMicsum_init: function BmkMicsumRec_init(uri, authenticator) {
    this._Bookmark_init(uri, authenticator);
    this.cleartext.type = "microsummary";
  },

  get generatorURI() this.cleartext.generatorURI,
  set generatorURI(value) {
    if (typeof(value) == "string")
      this.cleartext.generatorURI = value;
    else
      this.cleartext.generatorURI = value? value.spec : "";
  },

  get staticTitle() this.cleartext.staticTitle,
  set staticTitle(value) {
    this.cleartext.staticTitle = value;
  }
};

function BookmarkFolder(uri, authenticator) {
  this._BookmarkFolder_init(uri, authenticator);
}
BookmarkFolder.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Folder",

  _BookmarkFolder_init: function FolderRec_init(uri, authenticator) {
    this._PlacesItem_init(uri, authenticator);
    this.cleartext.type = "folder";
  },

  get title() this.cleartext.title,
  set title(value) {
    this.cleartext.title = value;
  }
};

function Livemark(uri, authenticator) {
  this._Livemark_init(uri, authenticator);
}
Livemark.prototype = {
  __proto__: BookmarkFolder.prototype,
  _logName: "Record.Livemark",

  _Livemark_init: function LvmkRec_init(uri, authenticator) {
    this._BookmarkFolder_init(uri, authenticator);
    this.cleartext.type = "livemark";
  },

  get siteURI() this.cleartext.siteURI,
  set siteURI(value) {
    if (typeof(value) == "string")
      this.cleartext.siteURI = value;
    else
      this.cleartext.siteURI = value? value.spec : "";
  },

  get feedURI() this.cleartext.feedURI,
  set feedURI(value) {
    if (typeof(value) == "string")
      this.cleartext.feedURI = value;
    else
      this.cleartext.feedURI = value? value.spec : "";
  }
};

function BookmarkSeparator(uri, authenticator) {
  this._BookmarkSeparator_init(uri, authenticator);
}
BookmarkSeparator.prototype = {
  __proto__: PlacesItem.prototype,
  _logName: "Record.Separator",

  _BookmarkSeparator_init: function SepRec_init(uri, authenticator) {
    this._PlacesItem_init(uri, authenticator);
    this.cleartext.type = "separator";
  }
};
