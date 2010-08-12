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
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/forms.js");
Cu.import("resource://services-sync/util.js");

let FormWrapper = {
  getAllEntries: function getAllEntries() {
    // Sort by (lastUsed - minLast) / (maxLast - minLast) * timesUsed / maxTimes
    let query = this.createStatement(
      "SELECT fieldname name, value FROM moz_formhistory " +
      "ORDER BY 1.0 * (lastUsed - (SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed ASC LIMIT 1)) / " +
        "((SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed DESC LIMIT 1) - (SELECT lastUsed FROM moz_formhistory ORDER BY lastUsed ASC LIMIT 1)) * " +
        "timesUsed / (SELECT timesUsed FROM moz_formhistory ORDER BY timesUsed DESC LIMIT 1) DESC " +
      "LIMIT 500");
    return Utils.queryAsync(query, ["name", "value"]);
  },

  getEntry: function getEntry(guid) {
    let query = this.createStatement(
      "SELECT fieldname name, value FROM moz_formhistory WHERE guid = :guid");
    query.params.guid = guid;
    return Utils.queryAsync(query, ["name", "value"])[0];
  },

  getGUID: function getGUID(name, value) {
    // Query for the provided entry
    let getQuery = this.createStatement(
      "SELECT guid FROM moz_formhistory " +
      "WHERE fieldname = :name AND value = :value");
    getQuery.params.name = name;
    getQuery.params.value = value;

    // Give the guid if we found one
    let item = Utils.queryAsync(getQuery, "guid")[0];
    if (item.guid != null)
      return item.guid;

    // We need to create a guid for this entry
    let setQuery = this.createStatement(
      "UPDATE moz_formhistory SET guid = :guid " +
      "WHERE fieldname = :name AND value = :value");
    let guid = Utils.makeGUID();
    setQuery.params.guid = guid;
    setQuery.params.name = name;
    setQuery.params.value = value;
    Utils.queryAsync(setQuery);

    return guid;
  },

  hasGUID: function hasGUID(guid) {
    let query = this.createStatement(
      "SELECT 1 FROM moz_formhistory WHERE guid = :guid");
    query.params.guid = guid;
    return Utils.queryAsync(query).length == 1;
  },

  replaceGUID: function replaceGUID(oldGUID, newGUID) {
    let query = this.createStatement(
      "UPDATE moz_formhistory SET guid = :newGUID WHERE guid = :oldGUID");
    query.params.oldGUID = oldGUID;
    query.params.newGUID = newGUID;
    Utils.queryAsync(query);
  },

  createStatement: function createStatement(query) {
    try {
      // Just return the statement right away if it's okay
      return Utils.createStatement(Svc.Form.DBConnection, query);
    }
    catch(ex) {
      // Assume guid column must not exist yet, so add it with an index
      Svc.Form.DBConnection.executeSimpleSQL(
        "ALTER TABLE moz_formhistory ADD COLUMN guid TEXT");
      Svc.Form.DBConnection.executeSimpleSQL(
        "CREATE INDEX IF NOT EXISTS moz_formhistory_guid_index " +
        "ON moz_formhistory (guid)");

      // Try creating the query now that the column exists
      return Utils.createStatement(Svc.Form.DBConnection, query);
    }
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
  get prefName() "history",

  _findDupe: function _findDupe(item) {
    if (Svc.Form.entryExists(item.name, item.value))
      return FormWrapper.getGUID(item.name, item.value);
  }
};

function FormStore(name) {
  Store.call(this, name);
}
FormStore.prototype = {
  __proto__: Store.prototype,

  getAllIDs: function FormStore_getAllIDs() {
    let guids = {};
    for each (let {name, value} in FormWrapper.getAllEntries())
      guids[FormWrapper.getGUID(name, value)] = true;
    return guids;
  },

  changeItemID: function FormStore_changeItemID(oldID, newID) {
    FormWrapper.replaceGUID(oldID, newID);
  },

  itemExists: function FormStore_itemExists(id) {
    return FormWrapper.hasGUID(id);
  },

  createRecord: function createRecord(guid, uri) {
    let record = new FormRec(uri);
    let entry = FormWrapper.getEntry(guid);
    if (entry != null) {
      record.name = entry.name;
      record.value = entry.value
    }
    else
      record.deleted = true;
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
    if (entry == null)
      return;

    Svc.Form.removeEntry(entry.name, entry.value);
  },

  update: function FormStore_update(record) {
    this._log.warn("Ignoring form record update request!");
  },

  wipe: function FormStore_wipe() {
    Svc.Form.removeAllEntries();
  }
};

function FormTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
FormTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFormSubmitObserver,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference]),

  trackEntry: function trackEntry(name, value) {
    this.addChangedID(FormWrapper.getGUID(name, value));
    this.score += 10;
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
      // Firefox 4.0
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
      // Firefox 3.5/3.6
      case "form-notifier":
        this.onFormNotifier(data);
        break;
    }
  },

  // Firefox 3.5/3.6
  onFormNotifier: function onFormNotifier(data) {
    let name, value;

    // Figure out if it's a function that we care about tracking
    let formCall = JSON.parse(data);
    let func = formCall.func;
    if ((func == "addEntry" && formCall.type == "after") ||
        (func == "removeEntry" && formCall.type == "before"))
      [name, value] = formCall.args;

    // Skip if there's nothing of interest
    if (name == null || value == null)
      return;

    this._log.trace("Logging form action: " + [func, name, value]);
    this.trackEntry(name, value);
  },

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

      // Get the GUID on a delay so that it can be added to the DB first...
      Utils.delay(function() {
        this._log.trace("Logging form element: " + [name, el.value]);
        this.trackEntry(name, el.value);
      }, 0, this);
    }
  }
};
