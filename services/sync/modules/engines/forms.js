/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["FormEngine", "FormRec", "FormValidator"];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/collection_validator.js");
Cu.import("resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

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

  _promiseSearch(terms, searchData) {
    return new Promise(resolve => {
      let results = [];
      let callbacks = {
        handleResult(result) {
          results.push(result);
        },
        handleCompletion(reason) {
          resolve(results);
        }
      };
      FormHistory.search(terms, searchData, callbacks);
    })
  },

  // Do a "sync" search by spinning the event loop until it completes.
  _searchSpinningly(terms, searchData) {
    return Async.promiseSpinningly(this._promiseSearch(terms, searchData));
  },

  _updateSpinningly(changes) {
    if (!FormHistory.enabled) {
      return; // update isn't going to do anything.
    }
    let cb = Async.makeSpinningCallback();
    let callbacks = {
      handleCompletion(reason) {
        cb();
      }
    };
    FormHistory.update(changes, callbacks);
    cb.wait();
  },

  getEntry(guid) {
    let results = this._searchSpinningly(this._getEntryCols, {guid});
    if (!results.length) {
      return null;
    }
    return {name: results[0].fieldname, value: results[0].value};
  },

  getGUID(name, value) {
    // Query for the provided entry.
    let query = { fieldname: name, value };
    let results = this._searchSpinningly(this._guidCols, query);
    return results.length ? results[0].guid : null;
  },

  hasGUID(guid) {
    // We could probably use a count function here, but searchSpinningly exists...
    return this._searchSpinningly(this._guidCols, {guid}).length != 0;
  },

  replaceGUID(oldGUID, newGUID) {
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

  _processChange(change) {
    // If this._changes is defined, then we are applying a batch, so we
    // can defer it.
    if (this._changes) {
      this._changes.push(change);
      return;
    }

    // Otherwise we must handle the change synchronously, right now.
    FormWrapper._updateSpinningly(change);
  },

  applyIncomingBatch(records) {
    // We collect all the changes to be made then apply them all at once.
    this._changes = [];
    let failures = Store.prototype.applyIncomingBatch.call(this, records);
    if (this._changes.length) {
      FormWrapper._updateSpinningly(this._changes);
    }
    delete this._changes;
    return failures;
  },

  getAllIDs() {
    let results = FormWrapper._searchSpinningly(["guid"], [])
    let guids = {};
    for (let result of results) {
      guids[result.guid] = true;
    }
    return guids;
  },

  changeItemID(oldID, newID) {
    FormWrapper.replaceGUID(oldID, newID);
  },

  itemExists(id) {
    return FormWrapper.hasGUID(id);
  },

  createRecord(id, collection) {
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

  create(record) {
    this._log.trace("Adding form record for " + record.name);
    let change = {
      op: "add",
      fieldname: record.name,
      value: record.value
    };
    this._processChange(change);
  },

  remove(record) {
    this._log.trace("Removing form record: " + record.id);
    let change = {
      op: "remove",
      guid: record.id
    };
    this._processChange(change);
  },

  update(record) {
    this._log.trace("Ignoring form record update request!");
  },

  wipe() {
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

  startTracking() {
    Svc.Obs.add("satchel-storage-changed", this);
  },

  stopTracking() {
    Svc.Obs.remove("satchel-storage-changed", this);
  },

  observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);
    if (this.ignoreAll) {
      return;
    }
    switch (topic) {
      case "satchel-storage-changed":
        if (data == "formhistory-add" || data == "formhistory-remove") {
          let guid = subject.QueryInterface(Ci.nsISupportsString).toString();
          this.trackEntry(guid);
        }
        break;
    }
  },

  trackEntry(guid) {
    if (this.addChangedID(guid)) {
      this.score += SCORE_INCREMENT_MEDIUM;
    }
  },
};


class FormsProblemData extends CollectionProblemData {
  getSummary() {
    // We don't support syncing deleted form data, so "clientMissing" isn't a problem
    return super.getSummary().filter(entry =>
      entry.name !== "clientMissing");
  }
}

class FormValidator extends CollectionValidator {
  constructor() {
    super("forms", "id", ["name", "value"]);
  }

  emptyProblemData() {
    return new FormsProblemData();
  }

  getClientItems() {
    return FormWrapper._promiseSearch(["guid", "fieldname", "value"], {});
  }

  normalizeClientItem(item) {
    return {
      id: item.guid,
      guid: item.guid,
      name: item.fieldname,
      fieldname: item.fieldname,
      value: item.value,
      original: item,
    };
  }

  normalizeServerItem(item) {
    let res = Object.assign({
      guid: item.id,
      fieldname: item.name,
      original: item,
    }, item);
    // Missing `name` or `value` causes the getGUID call to throw
    if (item.name !== undefined && item.value !== undefined) {
      let guid = FormWrapper.getGUID(item.name, item.value);
      if (guid) {
        res.guid = guid;
        res.id = guid;
        res.duped = true;
      }
    }

    return res;
  }
}
