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
 *  Myk Melez <myk@mozilla.org>
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

const EXPORTED_SYMBOLS = ['Engines', 'NewEngine', 'Engine', 'SyncEngine', 'BlobEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/clientData.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");

Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/collection.js");

Function.prototype.async = Async.sugar;

// Singleton service, holds registered engines

Utils.lazy(this, 'Engines', EngineManagerSvc);

function EngineManagerSvc() {
  this._engines = {};
}
EngineManagerSvc.prototype = {
  get: function EngMgr_get(name) {
    return this._engines[name];
  },
  getAll: function EngMgr_getAll() {
    let ret = [];
    for (key in this._engines) {
      ret.push(this._engines[key]);
    }
    return ret;
  },
  getEnabled: function EngMgr_getEnabled() {
    let ret = [];
    for (key in this._engines) {
      if(this._engines[key].enabled)
        ret.push(this._engines[key]);
    }
    return ret;
  },
  register: function EngMgr_register(engine) {
    this._engines[engine.name] = engine;
  },
  unregister: function EngMgr_unregister(val) {
    let name = val;
    if (val instanceof Engine)
      name = val.name;
    delete this._engines[name];
  }
};

function Engine() { /* subclasses should call this._init() */}
Engine.prototype = {
  _notify: Wrap.notify,

  // "default-engine";
  get name() { throw "name property must be overridden in subclasses"; },

  // "Default";
  get displayName() { throw "displayName property must be overriden in subclasses"; },

  // "DefaultEngine";
  get logName() { throw "logName property must be overridden in subclasses"; },

  // "user-data/default-engine/";
  get serverPrefix() { throw "serverPrefix property must be overridden in subclasses"; },

  get enabled() {
    return Utils.prefs.getBoolPref("engine." + this.name);
  },

  get _os() {
    let os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
    this.__defineGetter__("_os", function() os);
    return os;
  },

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].
      createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return json;
  },

  get score() this._tracker.score,

  // _store, and tracker need to be overridden in subclasses
  get _store() {
    let store = new Store();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _tracker() {
    let tracker = new Tracker();
    this.__defineGetter__("_tracker", function() tracker);
    return tracker;
  },

  get engineId() {
    let id = ID.get('Engine:' + this.name);
    if (!id) {
      // Copy the service login from WeaveID
      let masterID = ID.get('WeaveID');

      id = new Identity(this.logName, masterID.username, masterID.password);
      ID.set('Engine:' + this.name, id);
    }
    return id;
  },

  _init: function Engine__init() {
    let levelPref = "log.logger.service.engine." + this.name;
    let level = "Debug";
    try { level = Utils.prefs.getCharPref(levelPref); }
    catch (e) { /* ignore unset prefs */ }

    this._log = Log4Moz.repository.getLogger("Engine." + this.logName);
    this._log.level = Log4Moz.Level[level];
    this._osPrefix = "weave:" + this.name + ":";

    this._tracker; // initialize tracker to load previously changed IDs

    this._log.debug("Engine initialized");
  },

  _serializeCommands: function Engine__serializeCommands(commands) {
    let json = this._json.encode(commands);
    //json = json.replace(/ {action/g, "\n {action");
    return json;
  },

  _serializeConflicts: function Engine__serializeConflicts(conflicts) {
    let json = this._json.encode(conflicts);
    //json = json.replace(/ {action/g, "\n {action");
    return json;
  },

  _resetServer: function Engine__resetServer() {
    let self = yield;
    throw "_resetServer needs to be subclassed";
  },

  _resetClient: function Engine__resetClient() {
    let self = yield;
    this._log.debug("Resetting client state");
    this._store.wipe();
    this._log.debug("Client reset completed successfully");
  },

  _sync: function Engine__sync() {
    let self = yield;
    throw "_sync needs to be subclassed";
  },

  _share: function Engine__share(guid, username) {
    let self = yield;
    /* This should be overridden by the engine subclass for each datatype.
       Implementation should share the data node identified by guid,
       and all its children, if any, with the user identified by username. */
    self.done();
  },

  _stopSharing: function Engine__stopSharing(guid, username) {
    let self = yield;
    /* This should be overridden by the engine subclass for each datatype.
     Stop sharing the data node identified by guid with the user identified
     by username.*/
    self.done();
  },

  sync: function Engine_sync(onComplete) {
    return this._sync.async(this, onComplete);
  },

  share: function Engine_share(onComplete, guid, username) {
    return this._share.async(this, onComplete, guid, username);
  },

  stopSharing: function Engine_share(onComplete, guid, username) {
    return this._stopSharing.async(this, onComplete, guid, username);
  },

  resetServer: function Engimne_resetServer(onComplete) {
    this._notify("reset-server", "", this._resetServer).async(this, onComplete);
  },

  resetClient: function Engine_resetClient(onComplete) {
    this._notify("reset-client", "", this._resetClient).async(this, onComplete);
  }
};

function SyncEngine() { /* subclasses should call this._init() */ }
SyncEngine.prototype = {
  __proto__: Engine.prototype,

  get baseURL() {
    let url = Utils.prefs.getCharPref("serverURL");
    if (url && url[url.length-1] != '/')
      url = url + '/';
    return url;
  },

  get engineURL() {
    return this.baseURL + ID.get('WeaveID').username + '/' + this.name + '/';
  },

  get cryptoMetaURL() {
    return this.baseURL + ID.get('WeaveID').username + '/crypto/' + this.name;
  },

  get lastSync() {
    try {
      return Utils.prefs.getCharPref(this.name + ".lastSync");
    } catch (e) {
      return 0;
    }
  },
  set lastSync(value) {
    Utils.prefs.setCharPref(this.name + ".lastSync", value);
  },

  // XXX these two should perhaps just be a variable inside sync(), but we have
  //     one or two other methods that use it

  get incoming() {
    if (!this._incoming)
      this._incoming = [];
    return this._incoming;
  },

  get outgoing() {
    if (!this._outgoing)
      this._outgoing = [];
    return this._outgoing;
  },

  // Create a new record starting from an ID
  // Calls _store.wrapItem to get the actual item, but sometimes needs to be
  // overridden anyway (to alter parentid or other properties outside the payload)
  _createRecord: function SyncEngine__createRecord(id, encrypt) {
    let self = yield;

    let record = new CryptoWrapper();
    record.uri = this.engineURL + id;
    record.encryption = this.cryptoMetaURL;
    record.cleartext = this._store.wrapItem(id);

    if (record.cleartext && record.cleartext.parentid)
        record.parentid = record.cleartext.parentid;

    if (encrypt || encrypt == undefined)
      yield record.encrypt(self.cb, ID.get('WeaveCryptoID').password);

    self.done(record);
  },

  // Check if a record is "like" another one, even though the IDs are different,
  // in that case, we'll change the ID of the local item to match
  // Probably needs to be overridden in a subclass, to change which criteria
  // make two records "the same one"
  _recordLike: function SyncEngine__recordLike(a, b) {
    if (a.parentid != b.parentid)
      return false;
    return Utils.deepEquals(a.cleartext, b.cleartext);
  },

  _changeRecordRefs: function SyncEngine__changeRecordRefs(oldID, newID) {
    let self = yield;
    for each (let rec in this.outgoing) {
      if (rec.parentid == oldID)
        rec.parentid = newID;
    }
  },

  _recDepth: function SyncEngine__recDepth(rec) {
    // we've calculated depth for this record already
    if (rec.depth)
      return rec.depth;

    // record has no parent
    if (!rec.parentid)
      return 0;

    // search for the record's parent and calculate its depth, then add one
    for each (let inc in this.incoming) {
      if (inc.id == rec.parentid) {
        rec.depth = this._recDepth(inc) + 1;
        return rec.depth;
      }
    }

    // we couldn't find the record's parent, so it's an orphan
    return 0;
  },

  // Any setup that needs to happen at the beginning of each sync.
  // Makes sure crypto records and keys are all set-up
  _syncStartup: function SyncEngine__syncStartup() {
    let self = yield;

    this._log.debug("Ensuring server crypto records are there");

    let meta = yield CryptoMetas.get(self.cb, this.cryptoMetaURL);
    if (!meta) {
      let cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
        getService(Ci.IWeaveCrypto);
      let symkey = cryptoSvc.generateRandomKey();
      let pubkey = yield PubKeys.getDefaultKey(self.cb);
      meta = new CryptoMeta(this.cryptoMetaURL);
      meta.generateIV();
      yield meta.addUnwrappedKey(self.cb, pubkey, symkey);
      yield meta.put(self.cb);
    }
    this._tracker.disable();
  },

  // Generate outgoing records
  _generateOutgoing: function SyncEngine__generateOutgoing() {
    let self = yield;

    this._log.debug("Calculating client changes");

    // first sync special case: upload all items
    // note that we use a backdoor (of sorts) to the tracker and it
    // won't save to disk this list
    if (!this.lastSync) {
      this._log.info("First sync, uploading all items");

      // remove any old ones first
      this._tracker.clearChangedIDs();

      // now add all current ones
      let all = this._store.getAllIDs();
      for (let id in all) {
        this._tracker.changedIDs[id] = true;
      }
    }

    // generate queue from changed items list

    // XXX should have a heuristic like this, but then we need to be able to
    // serialize each item by itself, something our stores can't currently do
    //if (this._tracker.changedIDs.length >= 30)
    this._store.cacheItemsHint();

    // NOTE we want changed items -> outgoing -> server to be as atomic as
    // possible, so we clear the changed IDs after we upload the changed records
    // NOTE2 don't encrypt, we'll do that before uploading instead
    for (let id in this._tracker.changedIDs) {
      this.outgoing.push(yield this._createRecord.async(this, self.cb, id, false));
    }

    this._store.clearItemCacheHint();
  },

  // Generate outgoing records
  _fetchIncoming: function SyncEngine__fetchIncoming() {
    let self = yield;

    this._log.debug("Downloading server changes");

    let newitems = new Collection(this.engineURL);
    newitems.modified = this.lastSync;
    newitems.full = true;
    yield newitems.get(self.cb);

    let item;
    while ((item = yield newitems.iter.next(self.cb))) {
      this.incoming.push(item);
    }
  },

  // Process incoming records to get them ready for reconciliation and applying later
  // i.e., decrypt them, and sort them
  _processIncoming: function SyncEngine__processIncoming() {
    let self = yield;

    this._log.debug("Decrypting and sorting incoming changes");

    for each (let inc in this.incoming) {
      yield inc.decrypt(self.cb, ID.get('WeaveCryptoID').password);
      this._recDepth(inc); // note: doesn't need access to payload
    }
    this.incoming.sort(function(a, b) {
      if ((typeof(a.depth) == "number" && typeof(b.depth) == "undefined") ||
          (typeof(a.depth) == "number" && b.depth == null) ||
          (a.depth > b.depth))
        return 1;
      if ((typeof(a.depth) == "undefined" && typeof(b.depth) == "number") ||
          (a.depth == null && typeof(b.depth) == "number") ||
          (a.depth < b.depth))
        return -1;
      if (a.cleartext && b.cleartext) {
        if (a.cleartext.index > b.cleartext.index)
          return 1;
        if (a.cleartext.index < b.cleartext.index)
          return -1;
      }
      return 0;
    });
  },

  // Reconciliation has two steps:
  // 1) Check for the same item (same ID) on both the incoming and outgoing
  // queues.  This means the same item was modified on this profile and another
  // at the same time.  In this case, this client wins (which really means, the
  // last profile you sync wins).
  // 2) Check if any incoming & outgoing items are actually the same, even
  // though they have different IDs.  This happens when the same item is added
  // on two different machines at the same time.  For example, when a profile
  // is synced for the first time after having (manually or otherwise) imported
  // bookmarks imported, every bookmark will match this condition.
  // When two items with different IDs are "the same" we change the local ID to
  // match the remote one.
  _reconcile: function SyncEngine__reconcile() {
    let self = yield;

    this._log.debug("Reconciling server/client changes");

    this._log.debug(this.incoming.length + " items coming in, " +
                    this.outgoing.length + " items going out");

    // Check for the same item (same ID) on both incoming & outgoing queues
    let conflicts = [];
    for (let i = 0; i < this.incoming.length; i++) {
      for (let o = 0; o < this.outgoing.length; o++) {
        if (this.incoming[i].id == this.outgoing[o].id) {
          // Only consider it a conflict if there are actual differences
          // otherwise, just remove the outgoing record as well
          if (!Utils.deepEquals(this.incoming[i].cleartext,
                                this.outgoing[o].cleartext))
            conflicts.push({in: this.incoming[i], out: this.outgoing[o]});
          else
            delete this.outgoing[o];
          delete this.incoming[i];
          break;
        }
      }
      this._outgoing = this.outgoing.filter(function(n) n); // removes any holes
    }
    this._incoming = this.incoming.filter(function(n) n); // removes any holes
    if (conflicts.length)
      this._log.debug("Conflicts found.  Conflicting server changes discarded");

    // Check for items with different IDs which we think are the same one
    for (let i = 0; i < this.incoming.length; i++) {
      for (let o = 0; o < this.outgoing.length; o++) {
        if (this._recordLike(this.incoming[i], this.outgoing[o])) {
          // change refs in outgoing queue
          yield this._changeRecordRefs.async(this, self.cb,
                                             this.outgoing[o].id,
                                             this.incoming[i].id);
          // change actual id of item
          this._store.changeItemID(this.outgoing[o].id,
                                   this.incoming[i].id);
          delete this.incoming[i];
          delete this.outgoing[o];
          break;
        }
      }
      this._outgoing = this.outgoing.filter(function(n) n); // removes any holes
    }
    this._incoming = this.incoming.filter(function(n) n); // removes any holes

    this._log.debug("Reconciliation complete");
    this._log.debug(this.incoming.length + " items coming in, " +
                    this.outgoing.length + " items going out");
  },

  // Apply incoming records
  _applyIncoming: function SyncEngine__applyIncoming() {
    let self = yield;
    if (this.incoming.length) {
      this._log.debug("Applying server changes");
      let inc;
      while ((inc = this.incoming.shift())) {
        yield this._store.applyIncoming(self.cb, inc);
        if (inc.modified > this.lastSync)
          this.lastSync = inc.modified;
      }
    }
  },

  // Upload outgoing records
  _uploadOutgoing: function SyncEngine__uploadOutgoing() {
    let self = yield;
    if (this.outgoing.length) {
      this._log.debug("Uploading client changes");
      let up = new Collection(this.engineURL);
      let out;
      while ((out = this.outgoing.pop())) {
        yield out.encrypt(self.cb, ID.get('WeaveCryptoID').password);
        yield up.pushRecord(self.cb, out);
      }
      yield up.post(self.cb);
      if (up.data.modified > this.lastSync)
        this.lastSync = up.data.modified;
    }
    this._tracker.clearChangedIDs();
  },

  // Any cleanup necessary.
  // Save the current snapshot so as to calculate changes at next sync
  _syncFinish: function SyncEngine__syncFinish(error) {
    let self = yield;
    this._log.debug("Finishing up sync");
    this._tracker.resetScore();
    this._tracker.enable();
  },

  _sync: function SyncEngine__sync() {
    let self = yield;

    try {
      yield this._syncStartup.async(this, self.cb);

      // Populate incoming and outgoing queues
      yield this._generateOutgoing.async(this, self.cb);
      yield this._fetchIncoming.async(this, self.cb);

      // Decrypt and sort incoming records, then reconcile
      yield this._processIncoming.async(this, self.cb);
      yield this._reconcile.async(this, self.cb);

      // Apply incoming records, upload outgoing records
      yield this._applyIncoming.async(this, self.cb);
      yield this._uploadOutgoing.async(this, self.cb);

      yield this._syncFinish.async(this, self.cb);
    }
    catch (e) {
      this._log.warn("Sync failed");
      throw e;
    }
    finally {
      this._tracker.enable();
    }
  },

  _resetServer: function SyncEngine__resetServer() {
    let self = yield;
    let all = new Resource(this.engineURL);
    yield all.delete(self.cb);
  }
};

function BlobEngine() {
  // subclasses should call _init
  // we don't call it here because it requires serverPrefix to be set
}
BlobEngine.prototype = {
  __proto__: Engine.prototype,

  get _profileID() {
    return ClientData.GUID;
  },

  _init: function BlobEngine__init() {
    // FIXME meep?
    this.__proto__.__proto__.__proto__.__proto__._init.call(this);
    this._keys = new Keychain(this.serverPrefix);
    this._file = new Resource(this.serverPrefix + "data");
    this._file.pushFilter(new JsonFilter());
    this._file.pushFilter(new CryptoFilter(this.engineId));
  },

  _initialUpload: function BlobEngine__initialUpload() {
    let self = yield;
    this._log.info("Initial upload to server");
    yield this._keys.initialize(self.cb, this.engineId);
    this._file.data = {};
    yield this._merge.async(this, self.cb);
    yield this._file.put(self.cb);
  },

  // NOTE: Assumes this._file has latest server data
  // this method is separate from _sync so it's easy to override in subclasses
  _merge: function BlobEngine__merge() {
    let self = yield;
    this._file.data[this._profileID] = this._store.wrap();
  },

  // This engine is very simple:
  // 1) Get the latest server data
  // 2) Merge with our local data store
  // 3) Upload new merged data
  // NOTE: a version file will be needed in the future to optimize the case
  //       where there are no changes
  _sync: function BlobEngine__sync() {
    let self = yield;

    this._log.info("Beginning sync");
    this._os.notifyObservers(null, "weave:service:sync:engine:start", this.name);

    // FIXME
    //if (!(yield DAV.MKCOL(this.serverPrefix, self.cb)))
    //  throw "Could not create remote folder";

    try {
      if ("none" != Utils.prefs.getCharPref("encryption"))
        yield this._keys.getKeyAndIV(self.cb, this.engineId);
      yield this._file.get(self.cb);
      yield this._merge.async(this, self.cb);
      yield this._file.put(self.cb);

    } catch (e if e.status == 404) {
      yield this._initialUpload.async(this, self.cb);
    }

    this._log.info("Sync complete");
    this._os.notifyObservers(null, "weave:service:sync:engine:success", this.name);
    self.done(true);
  }
};

function HeuristicEngine() {
}
HeuristicEngine.prototype = {
  __proto__: new Engine(),

  get _remote() {
    let remote = new RemoteStore(this);
    this.__defineGetter__("_remote", function() remote);
    return remote;
  },

  get _snapshot() {
    let snap = new SnapshotStore(this.name);
    this.__defineGetter__("_snapshot", function() snap);
    return snap;
  },

  _resetServer: function SyncEngine__resetServer() {
    let self = yield;
    yield this._remote.wipe(self.cb);
  },

  _resetClient: function SyncEngine__resetClient() {
    let self = yield;
    this._log.debug("Resetting client state");
    this._snapshot.wipe();
    this._store.wipe();
    this._log.debug("Client reset completed successfully");
  },

  _initialUpload: function HeuristicEngine__initialUpload() {
    let self = yield;
    this._log.info("Initial upload to server");
    this._snapshot.data = this._store.wrap();
    this._snapshot.version = 0;
    this._snapshot.GUID = null; // in case there are other snapshots out there
    yield this._remote.initialize(self.cb, this._snapshot);
    this._snapshot.save();
  },

  _sync: function HeuristicEngine__sync() {
    let self = yield;
  }
};
