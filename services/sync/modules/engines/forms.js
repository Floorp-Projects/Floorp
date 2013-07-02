/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['FormEngine', 'FormRec'];

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

this.FormRec = function FormRec(collection, id) {
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

  get prefName() "history",

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

  _enabled: false,
  observe: function (subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Obs.add("form-notifier", this);
          Svc.Obs.add("satchel-storage-changed", this);
          // HTMLFormElement doesn't use the normal observer/observe
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
        if (data == "formhistory-add" || data == "formhistory-remove") {
          let guid = subject.QueryInterface(Ci.nsISupportsString).toString();
          this.trackEntry(guid);
        }
        break;
    case "profile-change-teardown":
      FormWrapper._finalize();
      break;
    }
  },

  trackEntry: function (guid) {
    this.addChangedID(guid);
    this.score += SCORE_INCREMENT_MEDIUM;
  },

  notify: function (formElement, aWindow, actionURI) {
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
        let guid = FormWrapper.getGUID(name, el.value);
        if (guid) {
          this.trackEntry(guid);
        }
      }, this);
    }
  }
};
