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

const EXPORTED_SYMBOLS = ['WBORecord', 'RecordManager', 'Records'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

function WBORecord(uri) {
  this._WBORec_init(uri);
}
WBORecord.prototype = {
  id: null,
  deleted: false,
  _logName: "Record.WBO",

  _WBORec_init: function WBORec_init(uri) {
    this.data = {
      payload: {}
    };
    if (uri)
      this.uri = uri;
  },

  // NOTE: baseUri must have a trailing slash, or baseUri.resolve() will omit
  //       the collection name
  get uri() {
    return Utils.makeURI(this.baseUri.resolve(encodeURI(this.id)));
  },
  set uri(value) {
    if (typeof(value) != "string")
      value = value.spec;
    let foo = value.split('/');
    this.id = foo.pop();
    this.baseUri = Utils.makeURI(foo.join('/') + '/');
  },

  get modified() {
    if (typeof(this.data.modified) == "string")
      this.data.modified = parseInt(this.data.modified);
    return this.data.modified;
  },

  get depth() {
    if (this.data.depth)
      return this.data.depth;
    return 0;
  },

  get sortindex() {
    if (this.data.sortindex)
      return this.data.sortindex;
    return 0;
  },

  serialize: function WBORec_serialize() {
    // Convert the payload into a string because the server expects that
    let payload = this.payload;
    this.payload = this.deleted ? "" : JSON.stringify(payload);

    let ret = JSON.stringify(this.data);

    // Restore the original payload
    this.payload = payload;

    return ret;
  },

  deserialize: function WBORec_deserialize(json) {
    this.data = JSON.parse(json);

    // Empty string payloads are deleted records
    if (this.payload === "")
      this.deleted = true;
    else
      this.payload = JSON.parse(this.payload);
  },

  toString: function WBORec_toString() "{ " + [
      "id: " + this.id,
      "parent: " + this.parentid,
      "depth: " + this.depth + ", index: " + this.sortindex,
      "modified: " + this.modified,
      "payload: " + (this.deleted ? "DELETED" : JSON.stringify(this.payload))
    ].join("\n  ") + " }",
};

Utils.deferGetSet(WBORecord, "data", ["parentid", "modified", "depth",
  "sortindex", "payload"]);

Utils.lazy(this, 'Records', RecordManager);

function RecordManager() {
  this._init();
}
RecordManager.prototype = {
  _recordType: WBORecord,
  _logName: "RecordMgr",

  _init: function RegordMgr__init() {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._records = {};
    this._aliases = {};
  },

  import: function RegordMgr_import(onComplete, url) {
    let fn = function RegordMgr__import(url) {
      let self = yield;
      let record;

      this._log.trace("Importing record: " + (url.spec? url.spec : url));

      try {
        this.lastResource = new Resource(url);
        this.lastResource.get();

        record = new this._recordType();
        record.deserialize(this.lastResource.data);
        record.uri = url; // NOTE: may override id in this.lastResource.data

        this.set(url, record);
      } catch (e) {
        this._log.debug("Failed to import record: " + e);
        record = null;
      }
      self.done(record);
    };
    fn.async(this, onComplete, url);
  },

  get: function RegordMgr_get(onComplete, url) {
    let fn = function RegordMgr__get(url) {
      let self = yield;

      let record = null;
      let spec = url.spec? url.spec : url;
      /* Note: using url object directly as key for this._records cache
       * does not work because different url objects (even pointing to the
       * same place) are different objects and therefore not equal.  So
       * always use the string, not the object, as a key.  TODO: use the
       * string as key for this._aliases as well?  (Don't know) */
      if (url in this._aliases)
        url = this._aliases[url];
      if (spec in this._records)
        record = this._records[spec];

      if (!record)
        record = yield this.import(self.cb, url);

      self.done(record);
    };
    fn.async(this, onComplete, url);
  },

  set: function RegordMgr_set(url, record) {
    let spec = url.spec ? url.spec : url;
    this._records[spec] = record;
  },

  contains: function RegordMgr_contains(url) {
    let record = null;
    if (url in this._aliases)
      url = this._aliases[url];
    if (url in this._records)
      return true;
    return false;
  },

  clearCache: function recordMgr_clearCache() {
    this._records = {};
  },

  del: function RegordMgr_del(url) {
    delete this._records[url];
  },
  getAlias: function RegordMgr_getAlias(alias) {
    return this._aliases[alias];
  },
  setAlias: function RegordMgr_setAlias(url, alias) {
    this._aliases[alias] = url;
  },
  delAlias: function RegordMgr_delAlias(alias) {
    delete this._aliases[alias];
  }
};
