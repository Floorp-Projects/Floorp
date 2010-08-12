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

const EXPORTED_SYMBOLS = ['Collection'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

function Collection(uri, recordObj) {
  Resource.call(this, uri);
  this._recordObj = recordObj;

  this._full = false;
  this._ids = null;
  this._limit = 0;
  this._older = 0;
  this._newer = 0;
  this._data = [];
}
Collection.prototype = {
  __proto__: Resource.prototype,
  _logName: "Collection",

  _rebuildURL: function Coll__rebuildURL() {
    // XXX should consider what happens if it's not a URL...
    this.uri.QueryInterface(Ci.nsIURL);

    let args = [];
    if (this.older)
      args.push('older=' + this.older);
    else if (this.newer) {
      args.push('newer=' + this.newer);
    }
    if (this.full)
      args.push('full=1');
    if (this.sort)
      args.push('sort=' + this.sort);
    if (this.ids != null)
      args.push("ids=" + this.ids);
    if (this.limit > 0 && this.limit != Infinity)
      args.push("limit=" + this.limit);

    this.uri.query = (args.length > 0)? '?' + args.join('&') : '';
  },

  // get full items
  get full() { return this._full; },
  set full(value) {
    this._full = value;
    this._rebuildURL();
  },

  // Apply the action to a certain set of ids
  get ids() this._ids,
  set ids(value) {
    this._ids = value;
    this._rebuildURL();
  },

  // Limit how many records to get
  get limit() this._limit,
  set limit(value) {
    this._limit = value;
    this._rebuildURL();
  },

  // get only items modified before some date
  get older() { return this._older; },
  set older(value) {
    this._older = value;
    this._rebuildURL();
  },

  // get only items modified since some date
  get newer() { return this._newer; },
  set newer(value) {
    this._newer = value;
    this._rebuildURL();
  },

  // get items sorted by some criteria. valid values:
  // oldest (oldest first)
  // newest (newest first)
  // index
  get sort() { return this._sort; },
  set sort(value) {
    this._sort = value;
    this._rebuildURL();
  },

  pushData: function Coll_pushData(data) {
    this._data.push(data);
  },

  clearRecords: function Coll_clearRecords() {
    this._data = [];
  },

  set recordHandler(onRecord) {
    // Save this because onProgress is called with this as the ChannelListener
    let coll = this;

    // Prepare a dummyUri so that records can generate the correct
    // relative URLs.  The last bit will be replaced with record.id.
    let dummyUri = this.uri.clone().QueryInterface(Ci.nsIURL);
    dummyUri.filePath += "/replaceme";
    dummyUri.query = "";

    // Switch to newline separated records for incremental parsing
    coll.setHeader("Accept", "application/newlines");

    this._onProgress = function() {
      let newline;
      while ((newline = this._data.indexOf("\n")) > 0) {
        // Split the json record from the rest of the data
        let json = this._data.slice(0, newline);
        this._data = this._data.slice(newline + 1);

        // Deserialize a record from json and give it to the callback
        let record = new coll._recordObj(dummyUri);
        record.deserialize(json);
        onRecord(record);
      }
    };
  }
};
