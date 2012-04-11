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

const EXPORTED_SYMBOLS = ['FormEngine', 'FormRec'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-common/log4moz.js");

const FORMS_TTL = 5184000; // 60 days

function FormRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
FormRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Form",
  ttl: FORMS_TTL
};

Utils.deferGetSet(FormRec, "cleartext", ["name", "value"]);


let FormWrapper = {
  _log: Log4Moz.repository.getLogger("Sync.Engine.Forms"),

  _getEntryCols: ["name", "value"],
  _guidCols:     ["guid"],

  _stmts: {},
  _getStmt: function _getStmt(query) {
    if (query in this._stmts) {
      return this._stmts[query];
    }

    this._log.trace("Creating SQL statement: " + query);
    let db = Svc.Form.DBConnection;
    return this._stmts[query] = db.createAsyncStatement(query);
  },

  _finalize : function () {
    for each (let stmt in FormWrapper._stmts) {
      stmt.finalize();
    }
    FormWrapper._stmts = {};
  },

  get _getAllEntriesStmt() {
    const query =
      "SELECT fieldname name, value FROM moz_formhistory " +
      "ORDER BY 1.0 * (lastUsed - (SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed ASC LIMIT 1)) / " +
        "((SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed DESC LIMIT 1) - (SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed ASC LIMIT 1)) * " +
        "timesUsed / (SELECT timesUsed FROM moz_formhistory ORDER BY timesUsed DESC LIMIT 1) DESC " +
      "LIMIT 500";
    return this._getStmt(query);
  },

  get _getEntryStmt() {
    const query = "SELECT fieldname name, value FROM moz_formhistory " +
                  "WHERE guid = :guid";
    return this._getStmt(query);
  },

  get _getGUIDStmt() {
    const query = "SELECT guid FROM moz_formhistory " +
                  "WHERE fieldname = :name AND value = :value";
    return this._getStmt(query);
  },

  get _setGUIDStmt() {
    const query = "UPDATE moz_formhistory SET guid = :guid " +
                  "WHERE fieldname = :name AND value = :value";
    return this._getStmt(query);
  },

  get _hasGUIDStmt() {
    const query = "SELECT guid FROM moz_formhistory WHERE guid = :guid LIMIT 1";
    return this._getStmt(query);
  },

  get _replaceGUIDStmt() {
    const query = "UPDATE moz_formhistory SET guid = :newGUID " +
                  "WHERE guid = :oldGUID";
    return this._getStmt(query);
  },

  getAllEntries: function getAllEntries() {
    return Async.querySpinningly(this._getAllEntriesStmt, this._getEntryCols);
  },

  getEntry: function getEntry(guid) {
    let stmt = this._getEntryStmt;
    stmt.params.guid = guid;
    return Async.querySpinningly(stmt, this._getEntryCols)[0];
  },

  getGUID: function getGUID(name, value) {
    // Query for the provided entry.
    let getStmt = this._getGUIDStmt;
    getStmt.params.name = name;
    getStmt.params.value = value;

    // Give the GUID if we found one.
    let item = Async.querySpinningly(getStmt, this._guidCols)[0];

    if (!item) {
      // Shouldn't happen, but Bug 597400...
      // Might as well just return.
      this._log.warn("GUID query returned " + item + "; turn on Trace logging for details.");
      this._log.trace("getGUID(" + JSON.stringify(name) + ", " +
                      JSON.stringify(value) + ") => " + item);
      return null;
    }

    if (item.guid != null) {
      return item.guid;
    }

    // We need to create a GUID for this entry.
    let setStmt = this._setGUIDStmt;
    let guid = Utils.makeGUID();
    setStmt.params.guid = guid;
    setStmt.params.name = name;
    setStmt.params.value = value;
    Async.querySpinningly(setStmt);

    return guid;
  },

  hasGUID: function hasGUID(guid) {
    let stmt = this._hasGUIDStmt;
    stmt.params.guid = guid;
    return Async.querySpinningly(stmt, this._guidCols).length == 1;
  },

  replaceGUID: function replaceGUID(oldGUID, newGUID) {
    let stmt = this._replaceGUIDStmt;
    stmt.params.oldGUID = oldGUID;
    stmt.params.newGUID = newGUID;
    Async.querySpinningly(stmt);
  }

};

function FormEngine() {
  SyncEngine.call(this, "Forms");
}
FormEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: FormStore,
  _trackerObj: FormTracker,
  _recordObj: FormRec,
  applyIncomingBatchSize: FORMS_STORE_BATCH_SIZE,

  get prefName() "history",

  _findDupe: function _findDupe(item) {
    if (Svc.Form.entryExists(item.name, item.value)) {
      return FormWrapper.getGUID(item.name, item.value);
    }
  }
};

function FormStore(name) {
  Store.call(this, name);
}
FormStore.prototype = {
  __proto__: Store.prototype,

  applyIncomingBatch: function applyIncomingBatch(records) {
    return Utils.runInTransaction(Svc.Form.DBConnection, function() {
      return Store.prototype.applyIncomingBatch.call(this, records);
    }, this);
  },

  applyIncoming: function applyIncoming(record) {
    Store.prototype.applyIncoming.call(this, record);
    this._sleep(0); // Yield back to main thread after synchronous operation.
  },

  getAllIDs: function FormStore_getAllIDs() {
    let guids = {};
    for each (let {name, value} in FormWrapper.getAllEntries()) {
      guids[FormWrapper.getGUID(name, value)] = true;
    }
    return guids;
  },

  changeItemID: function FormStore_changeItemID(oldID, newID) {
    FormWrapper.replaceGUID(oldID, newID);
  },

  itemExists: function FormStore_itemExists(id) {
    return FormWrapper.hasGUID(id);
  },

  createRecord: function createRecord(id, collection) {
    let record = new FormRec(collection, id);
    let entry = FormWrapper.getEntry(id);
    if (entry != null) {
      record.name = entry.name;
      record.value = entry.value;
    } else {
      record.deleted = true;
    }
    return record;
  },

  create: function FormStore_create(record) {
    this._log.trace("Adding form record for " + record.name);
    Svc.Form.addEntry(record.name, record.value);
  },

  remove: function FormStore_remove(record) {
    this._log.trace("Removing form record: " + record.id);

    // Just skip remove requests for things already gone
    let entry = FormWrapper.getEntry(record.id);
    if (entry == null) {
      return;
    }

    Svc.Form.removeEntry(entry.name, entry.value);
  },

  update: function FormStore_update(record) {
    this._log.trace("Ignoring form record update request!");
  },

  wipe: function FormStore_wipe() {
    Svc.Form.removeAllEntries();
  }
};

function FormTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
  Svc.Obs.add("profile-change-teardown", this);
}
FormTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFormSubmitObserver,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference]),

  trackEntry: function trackEntry(name, value) {
    this.addChangedID(FormWrapper.getGUID(name, value));
    this.score += SCORE_INCREMENT_MEDIUM;
  },

  _enabled: false,
  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Obs.add("form-notifier", this);
          Svc.Obs.add("satchel-storage-changed", this);
          // nsHTMLFormElement doesn't use the normal observer/observe
          // pattern and looks up nsIFormSubmitObservers to .notify()
          // them so add manually to observers
          Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService)
            .addObserver(this, "earlyformsubmit", true);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.Obs.remove("form-notifier", this);
          Svc.Obs.remove("satchel-storage-changed", this);
          Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService)
            .removeObserver(this, "earlyformsubmit");
          this._enabled = false;
        }
        break;
      case "satchel-storage-changed":
        if (data == "addEntry" || data == "before-removeEntry") {
          subject = subject.QueryInterface(Ci.nsIArray);
          let name = subject.queryElementAt(0, Ci.nsISupportsString)
                            .toString();
          let value = subject.queryElementAt(1, Ci.nsISupportsString)
                             .toString();
          this.trackEntry(name, value);
        }
        break;
    case "profile-change-teardown":
      FormWrapper._finalize();
      break;
    }
  },

  notify: function FormTracker_notify(formElement, aWindow, actionURI) {
    if (this.ignoreAll) {
      return;
    }

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
      if (name === "") {
        name = el.id;
      }

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

      // Get the GUID on a delay so that it can be added to the DB first...
      Utils.nextTick(function() {
        this._log.trace("Logging form element: " + [name, el.value]);
        this.trackEntry(name, el.value);
      }, this);
    }
  }
};
