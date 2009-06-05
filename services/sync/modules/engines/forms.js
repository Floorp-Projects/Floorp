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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Anant Narayanan <anant@kix.in>
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

const EXPORTED_SYMBOLS = ['FormEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/type_records/forms.js");

function FormEngine() {
  this._init();
}
FormEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "forms",
  displayName: "Forms",
  logName: "Forms",
  _storeObj: FormStore,
  _trackerObj: FormTracker,
  _recordObj: FormRec,

  _syncStartup: function FormEngine__syncStartup() {
    this._store.cacheFormItems();
    SyncEngine.prototype._syncStartup();
  },

  /* Wipe cache when sync finishes */
  _syncFinish: function FormEngine__syncFinish() {
    this._store.clearFormCache();
    SyncEngine.prototype._syncFinish();
  },
  
  _recordLike: function SyncEngine__recordLike(a, b) {
    if (a.deleted || b.deleted)
      return false;
    if (a.name == b.name && a.value == b.value)
      return true;
    return false;
  }
};


function FormStore() {
  this._init();
}
FormStore.prototype = {
  __proto__: Store.prototype,
  _logName: "FormStore",
  _formItems: null,

  get _formDB() {
    let file = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).
      get("ProfD", Ci.nsIFile);
    file.append("formhistory.sqlite");
    let stor = Cc["@mozilla.org/storage/service;1"].
      getService(Ci.mozIStorageService);
    let formDB = stor.openDatabase(file);
      
    this.__defineGetter__("_formDB", function() formDB);
    return formDB;
  },

  get _formHistory() {
    let formHistory = Cc["@mozilla.org/satchel/form-history;1"].
      getService(Ci.nsIFormHistory2);
    this.__defineGetter__("_formHistory", function() formHistory);
    return formHistory;
  },
  
  get _formStatement() {
    let stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");
    this.__defineGetter__("_formStatement", function() stmnt);
    return stmnt;
  },
  
  cacheFormItems: function FormStore_cacheFormItems() {
    this._log.debug("Caching all form items");
    this._formItems = this.getAllIDs();
  },
  
  clearFormCache: function FormStore_clearFormCache() {
    this._log.debug("Clearing form cache");
    this._formItems = null;
  },
  
  getAllIDs: function FormStore_getAllIDs() {
    let items = {};
    let stmnt = this._formStatement;

    while (stmnt.executeStep()) {
      let nam = stmnt.getUTF8String(1);
      let val = stmnt.getUTF8String(2);
      let key = Utils.sha1(nam + val);

      items[key] = { name: nam, value: val };
    }
    stmnt.reset();

    return items;
  },
  
  changeItemID: function FormStore_changeItemID(oldID, newID) {
    this._log.warn("FormStore IDs are data-dependent, cannot change!");
  },
  
  itemExists: function FormStore_itemExists(id) {
    return (id in this._formItems);
  },
  
  createRecord: function FormStore_createRecord(guid, cryptoMetaURL) {
    let record = new FormRec();
    record.id = guid;
    
    if (guid in this._formItems) {
      let item = this._formItems[guid];
      record.encryption = cryptoMetaURL;
      record.name = item.name;
      record.value = item.value;
    } else {
      record.deleted = true;
    }
    
    return record;
  },
  
  create: function FormStore_create(record) {
    this._log.debug("Adding form record for " + record.name);
    this._formHistory.addEntry(record.name, record.value);
  },

  remove: function FormStore_remove(record) {
    this._log.debug("Removing form record: " + record.id);
    
    if (record.id in this._formItems) {
      let item = this._formItems[record.id];
      this._formHistory.removeEntry(item.name, item.value);
      return;
    }
    
    this._log.warn("Invalid GUID found, ignoring remove request.");
  },

  update: function FormStore_update(record) {
    this._log.warn("Ignoring form record update request!");
  },
  
  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  }
};

function FormTracker() {
  this._init();
}
FormTracker.prototype = {
  __proto__: Tracker.prototype,
  _logName: "FormTracker",
  file: "form",
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),
  
  __observerService: null,
  get _observerService() {
    if (!this.__observerService)
      this.__observerService = Cc["@mozilla.org/observer-service;1"].
                                getService(Ci.nsIObserverService);
      return this.__observerService;
  },
  
  _init: function FormTracker__init() {
    this.__proto__.__proto__._init.call(this);
    this._log.trace("FormTracker initializing!");
    this._observerService.addObserver(this, "earlyformsubmit", false);
  },
  
  /* 10 points per form element */
  notify: function FormTracker_notify(formElement, aWindow, actionURI) {
    if (this.ignoreAll)
      return;

    this._log.trace("Form submission notification for " + actionURI.spec);

    // XXX Bug 487541 Copy the logic from nsFormHistory::Notify to avoid
    // divergent logic, which can lead to security issues, until there's a
    // better way to get satchel's results like with a notification.

    // Determine if a dom node has the autocomplete attribute set to "off"
    let completeOff = function(domNode) {
      let autocomplete = domNode.getAttribute("autocomplete");
      return autocomplete && autocomplete.search(/^off$/i) == 0;
    }

    if (completeOff(formElement)) {
      this._log.trace("Form autocomplete set to off");
      return;
    }

    /* Get number of elements in form, add points and changedIDs */
    let len = formElement.length;
    let elements = formElement.elements;
    for (let i = 0; i < len; i++) {
      let el = elements.item(i);

      // Grab the name for debugging, but check if empty when satchel would
      let name = el.name;
      if (name === "")
        name = el.id;

      if (!(el instanceof Ci.nsIDOMHTMLInputElement)) {
        this._log.trace(name + " is not a DOMHTMLInputElement: " + el);
        continue;
      }

      if (el.type.search(/^text$/i) != 0) {
        this._log.trace(name + "'s type is not 'text': " + el.type);
        continue;
      }

      if (completeOff(el)) {
        this._log.trace(name + "'s autocomplete set to off");
        continue;
      }

      if (el.value === "") {
        this._log.trace(name + "'s value is empty");
        continue;
      }

      if (el.value == el.defaultValue) {
        this._log.trace(name + "'s value is the default");
        continue;
      }

      if (name === "") {
        this._log.trace("Text input element has no name or id");
        continue;
      }

      this._log.trace("Logging form element: " + name + " :: " + el.value);
      this.addChangedID(Utils.sha1(name + el.value));
      this._score += 10;
    }
  }
};
