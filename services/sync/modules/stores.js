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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

const EXPORTED_SYMBOLS = ["Store"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

/*
 * Data Stores
 * These can wrap, serialize items and apply commands
 */

function Store(name) {
  name = name || "Unnamed";
  this.name = name.toLowerCase();

  this._log = Log4Moz.repository.getLogger("Store." + name);
  let level = Svc.Prefs.get("log.logger.engine." + this.name, "Debug");
  this._log.level = Log4Moz.Level[level];
}
Store.prototype = {
  get cache() {
    let cache = new Cache();
    this.__defineGetter__("cache", function() cache);
    return cache;
  },

  applyIncoming: function Store_applyIncoming(record) {
    if (record.deleted)
      this.remove(record);
    else if (!this.itemExists(record.id))
      this.create(record);
    else
      this.update(record);
  },

  // override these in derived objects

  create: function Store_create(record) {
    throw "override create in a subclass";
  },

  remove: function Store_remove(record) {
    throw "override remove in a subclass";
  },

  update: function Store_update(record) {
    throw "override update in a subclass";
  },

  itemExists: function Store_itemExists(id) {
    throw "override itemExists in a subclass";
  },

  createRecord: function Store_createRecord(id) {
    throw "override createRecord in a subclass";
  },

  changeItemID: function Store_changeItemID(oldID, newID) {
    throw "override changeItemID in a subclass";
  },

  getAllIDs: function Store_getAllIDs() {
    throw "override getAllIDs in a subclass";
  },

  wipe: function Store_wipe() {
    throw "override wipe in a subclass";
  }
};

function Cache() {
  this.count = 0;
  this.maxItems = 100;
  this.fifo = true;
  this.enabled = true;
  this._head = this._tail = null;
  this._items = {};
}
Cache.prototype = {
  remove: function Cache_remove(item) {
    if (this.count <= 0 || this.count == 1) {
      this.clear();
      return;
    }

    item.next.prev = item.prev;
    item.prev.next = item.next;

    if (item == this._head)
      this._head = item.next;
    if (item == this._tail)
      this._tail = item.prev;

    item.next = null;
    item.prev = null;

    delete this._items[item.id];
    this.count--;
  },
  pop: function Cache_pop() {
    if (this.fifo)
      this.remove(this._tail);
    else
      this.remove(this._head);
  },
  put: function Cache_put(id, item) {
    if (!this.enabled)
      return;

    let wrapper = {id: id, item: item};

    if (this._head === null) {
      wrapper.next = wrapper;
      wrapper.prev = wrapper;
      this._head = wrapper;
      this._tail = wrapper;
    } else {
      wrapper.next = this._tail;
      wrapper.prev = this._head;
      this._head.prev = wrapper;
      this._tail.next = wrapper;
      this._tail = wrapper;
    }

    this._items[wrapper.id] = wrapper;
    this.count++;

    if (this.count >= this.maxItems)
      this.pop();
  },
  get: function Cache_get(id) {
    if (id in this._items)
      return this._items[id].item;
    return undefined;
  },
  clear: function Cache_clear() {
    this.count = 0;
    this._head = null;
    this._tail = null;
    this._items = {};
  }
};
