/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['FormEngine', 'FormRec'];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Log.jsm");

const FORMS_TTL = 3 * 365 * 24 * 60 * 60;   // Three years in seconds.

this.FormRec = function FormRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
FormRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Form",
  ttl: FORMS_TTL
};

Utils.deferGetSet(FormRec, "cleartext", ["name", "value"]);


var FormWrapper = {
  _log: Log.repository.getLogger("Sync.Engine.Forms"),

  _getEntryCols: ["fieldname", "value"],
  _guidCols:     ["guid"],

  // Do a "sync" search by spinning the event loop until it completes.
  _searchSpinningly: function(terms, searchData) {
    let results = [];
    let cb = Async.makeSpinningCallback();
    let callbacks = {
      handleResult: function(result) {
        results.push(result);
      },
      handleCompletion: function(reason) {
        cb(null, results);
      }
    };
    Svc.FormHistory.search(terms, searchData, callbacks);
    return cb.wait();
  },

  _updateSpinningly: function(changes) {
    if (!Svc.FormHistory.enabled) {
      return; // update isn't going to do anything.
    }
    let cb = Async.makeSpinningCallback();
    let callbacks = {
      handleCompletion: function(reason) {
        cb();
      }
    };
    Svc.FormHistory.update(changes, callbacks);
    return cb.wait();
  },

  getEntry: function (guid) {
    let results = this._searchSpinningly(this._getEntryCols, {guid: guid});
    if (!results.length) {
      return null;
    }
    return {name: results[0].fieldname, value: results[0].value};
  },

  getGUID: function (name, value) {
    // Query for the provided entry.
    let query = { fieldname: name, value: value };
    let results = this._searchSpinningly(this._guidCols, query);
    return results.length ? results[0].guid : null;
  },

  hasGUID: function (guid) {
    // We could probably use a count function here, but searchSpinningly exists...
    return this._searchSpinningly(this._guidCols, {guid: guid}).length != 0;
  },

  replaceGUID: function (oldGUID, newGUID) {
    let changes = {
      op: "update",
      guid: oldGUID,
      newGuid: newGUID,
    }
    this._updateSpinningly(changes);
  }

};

this.FormEngine = function FormEngine(service) {
  SyncEngine.call(this, "Forms", service);
}
FormEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: FormStore,
  _trackerObj: FormTracker,
  _recordObj: FormRec,
  applyIncomingBatchSize: FORMS_STORE_BATCH_SIZE,

  syncPriority: 6,

  get prefName() {
    return "history";
  },

  _findDupe: function _findDupe(item) {
    return FormWrapper.getGUID(item.name, item.value);
  }
};

function FormStore(name, engine) {
  Store.call(this, name, engine);
}
FormStore.prototype = {
  __proto__: Store.prototype,

  _processChange: function (change) {
    // If this._changes is defined, then we are applying a batch, so we
    // can defer it.
    if (this._changes) {
      this._changes.push(change);
      return;
    }

    // Otherwise we must handle the change synchronously, right now.
    FormWrapper._updateSpinningly(change);
  },

  applyIncomingBatch: function (records) {
    // We collect all the changes to be made then apply them all at once.
    this._changes = [];
    let failures = Store.prototype.applyIncomingBatch.call(this, records);
    if (this._changes.length) {
      FormWrapper._updateSpinningly(this._changes);
    }
    delete this._changes;
    return failures;
  },

  getAllIDs: function () {
    let results = FormWrapper._searchSpinningly(["guid"], [])
    let guids = {};
    for (let result of results) {
      guids[result.guid] = true;
    }
    return guids;
  },

  changeItemID: function (oldID, newID) {
    FormWrapper.replaceGUID(oldID, newID);
  },

  itemExists: function (id) {
    return FormWrapper.hasGUID(id);
  },

  createRecord: function (id, collection) {
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

  create: function (record) {
    this._log.trace("Adding form record for " + record.name);
    let change = {
      op: "add",
      fieldname: record.name,
      value: record.value
    };
    this._processChange(change);
  },

  remove: function (record) {
    this._log.trace("Removing form record: " + record.id);
    let change = {
      op: "remove",
      guid: record.id
    };
    this._processChange(change);
  },

  update: function (record) {
    this._log.trace("Ignoring form record update request!");
  },

  wipe: function () {
    let change = {
      op: "remove"
    };
    FormWrapper._updateSpinningly(change);
  }
};

function FormTracker(name, engine) {
  Tracker.call(this, name, engine);
}
FormTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference]),

  startTracking: function() {
    Svc.Obs.add("satchel-storage-changed", this);
  },

  stopTracking: function() {
    Svc.Obs.remove("satchel-storage-changed", this);
  },

  observe: function (subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "satchel-storage-changed":
        if (data == "formhistory-add" || data == "formhistory-remove") {
          let guid = subject.QueryInterface(Ci.nsISupportsString).toString();
          this.trackEntry(guid);
        }
        break;
    }
  },

  trackEntry: function (guid) {
    this.addChangedID(guid);
    this.score += SCORE_INCREMENT_MEDIUM;
  },
};
