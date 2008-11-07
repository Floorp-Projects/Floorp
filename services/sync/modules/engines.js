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
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");

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

function Engine() {}
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

  // _core, _store and _tracker need to be overridden in subclasses
  get _store() {
    let store = new Store();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _core() {
    let core = new SyncCore(this._store);
    this.__defineGetter__("_core", function() core);
    return core;
  },

  get _tracker() {
    let tracker = new tracker();
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

    this._log = Log4Moz.repository.getLogger("Service." + this.logName);
    this._log.level = Log4Moz.Level[level];
    this._osPrefix = "weave:" + this.name + ":";
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

  _initialUpload: function Engine__initialUpload() {
    let self = yield;
    throw "_initialUpload needs to be subclassed";
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

function NewEngine() {}
NewEngine.prototype = {
  __proto__: Engine.prototype,

  _sync: function NewEngine__sync() {
    let self = yield;
    self.done();
  }
};

function SyncEngine() {}
SyncEngine.prototype = {
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

  _initialUpload: function Engine__initialUpload() {
    let self = yield;
    this._log.info("Initial upload to server");
    this._snapshot.data = this._store.wrap();
    this._snapshot.version = 0;
    this._snapshot.GUID = null; // in case there are other snapshots out there
    yield this._remote.initialize(self.cb, this._snapshot);
    this._snapshot.save();
  },

  //       original
  //         / \
  //      A /   \ B
  //       /     \
  // client --C-> server
  //       \     /
  //      D \   / C
  //         \ /
  //        final

  // If we have a saved snapshot, original == snapshot.  Otherwise,
  // it's the empty set {}.

  // C is really the diff between server -> final, so if we determine
  // D we can calculate C from that.  In the case where A and B have
  // no conflicts, C == A and D == B.

  // Sync flow:
  // 1) Fetch server deltas
  // 1.1) Construct current server status from snapshot + server deltas
  // 1.2) Generate single delta from snapshot -> current server status ("B")
  // 2) Generate local deltas from snapshot -> current client status ("A")
  // 3) Reconcile client/server deltas and generate new deltas for them.
  //    Reconciliation won't generate C directly, we will simply diff
  //    server->final after step 3.1.
  // 3.1) Apply local delta with server changes ("D")
  // 3.2) Append server delta to the delta file and upload ("C")

  _sync: function SyncEngine__sync() {
    let self = yield;

    this._log.info("Beginning sync");
    this._os.notifyObservers(null, "weave:service:sync:engine:start", this.displayName);

    this._snapshot.load();

    try {
      this._remote.status.data; // FIXME - otherwise we get an error...
      yield this._remote.openSession(self.cb, this._snapshot);

    } catch (e if e.status == 404) {
      yield this._initialUpload.async(this, self.cb);
      return;
    }

    // 1) Fetch server deltas

    this._os.notifyObservers(null, "weave:service:sync:status", "status.downloading-deltas");
    let serverSnap = yield this._remote.wrap(self.cb);
    let serverUpdates = yield this._core.detectUpdates(self.cb,
                                                       this._snapshot.data, serverSnap);

    // 2) Generate local deltas from snapshot -> current client status

    this._os.notifyObservers(null, "weave:service:sync:status", "status.calculating-differences");
    let localSnap = new SnapshotStore();
    localSnap.data = this._store.wrap();
    this._core.detectUpdates(self.cb, this._snapshot.data, localSnap.data);
    let localUpdates = yield;

    this._log.trace("local json:\n" + localSnap.serialize());
    this._log.trace("Local updates: " + this._serializeCommands(localUpdates));
    this._log.trace("Server updates: " + this._serializeCommands(serverUpdates));

    if (serverUpdates.length == 0 && localUpdates.length == 0) {
      this._os.notifyObservers(null, "weave:service:sync:status", "status.no-changes-required");
      this._log.info("Sync complete: no changes needed on client or server");
      this._snapshot.version = this._remote.status.data.maxVersion;
      this._snapshot.save();
      self.done(true);
      return;
    }

    // 3) Reconcile client/server deltas and generate new deltas for them.

    this._os.notifyObservers(null, "weave:service:sync:status", "status.reconciling-updates");
    this._log.info("Reconciling client/server updates");
    let ret = yield this._core.reconcile(self.cb, localUpdates, serverUpdates);

    let clientChanges = ret.propagations[0];
    let serverChanges = ret.propagations[1];
    let clientConflicts = ret.conflicts[0];
    let serverConflicts = ret.conflicts[1];

    this._log.info("Changes for client: " + clientChanges.length);
    this._log.info("Predicted changes for server: " + serverChanges.length);
    this._log.info("Client conflicts: " + clientConflicts.length);
    this._log.info("Server conflicts: " + serverConflicts.length);
    this._log.trace("Changes for client: " + this._serializeCommands(clientChanges));
    this._log.trace("Predicted changes for server: " + this._serializeCommands(serverChanges));
    this._log.trace("Client conflicts: " + this._serializeConflicts(clientConflicts));
    this._log.trace("Server conflicts: " + this._serializeConflicts(serverConflicts));

    if (!(clientChanges.length || serverChanges.length ||
          clientConflicts.length || serverConflicts.length)) {
      this._os.notifyObservers(null, "weave:service:sync:status", "status.no-changes-required");
      this._log.info("Sync complete: no changes needed on client or server");
      this._snapshot.data = localSnap.data;
      this._snapshot.version = this._remote.status.data.maxVersion;
      this._snapshot.save();
      self.done(true);
      return;
    }

    if (clientConflicts.length || serverConflicts.length)
      this._log.warn("Conflicts found!  Discarding server changes");

    // 3.1) Apply server changes to local store

    if (clientChanges.length) {
      this._log.info("Applying changes locally");
      this._os.notifyObservers(null, "weave:service:sync:status", "status.applying-changes");

      // apply to real store
      yield this._store.applyCommands.async(this._store, self.cb, clientChanges);

      // get the current state
      let newSnap = new SnapshotStore();
      newSnap.data = this._store.wrap();

      // apply to the snapshot we got in step 1, and compare with current state
      yield localSnap.applyCommands.async(localSnap, self.cb, clientChanges);
      let diff = yield this._core.detectUpdates(self.cb,
                                                localSnap.data, newSnap.data);
      if (diff.length != 0) {
        this._log.warn("Commands did not apply correctly");
        this._log.trace("Diff from snapshot+commands -> " +
                        "new snapshot after commands:\n" +
                        this._serializeCommands(diff));
      }

      // update the local snap to the current state, we'll use it below
      localSnap.data = newSnap.data;
      localSnap.version = this._remote.status.data.maxVersion;
    }

    // 3.2) Append server delta to the delta file and upload

    // Generate a new diff, from the current server snapshot to the
    // current client snapshot.  In the case where there are no
    // conflicts, it should be the same as what the resolver returned

    this._os.notifyObservers(null, "weave:service:sync:status",
                             "status.calculating-differences");
    let serverDelta = yield this._core.detectUpdates(self.cb,
                                                     serverSnap, localSnap.data);

    // Log an error if not the same
    if (!(serverConflicts.length ||
          Utils.deepEquals(serverChanges, serverDelta)))
      this._log.warn("Predicted server changes differ from " +
                     "actual server->client diff (can be ignored in many cases)");

    this._log.info("Actual changes for server: " + serverDelta.length);
    this._log.trace("Actual changes for server: " +
                    this._serializeCommands(serverDelta));

    if (serverDelta.length) {
      this._log.info("Uploading changes to server");
      this._os.notifyObservers(null, "weave:service:sync:status",
                               "status.uploading-deltas");

      yield this._remote.appendDelta(self.cb, localSnap, serverDelta,
                                     {maxVersion: this._snapshot.version,
                                      deltasEncryption: Crypto.defaultAlgorithm});
      localSnap.version = this._remote.status.data.maxVersion;

      this._log.info("Successfully updated deltas and status on server");
    }

    this._snapshot.data = localSnap.data;
    this._snapshot.version = localSnap.version;
    this._snapshot.save();

    this._log.info("Sync complete");
    self.done(true);
  }
};

function BlobEngine() {
  // subclasses should call _init
  // we don't call it here because it requires serverPrefix to be set
}
BlobEngine.prototype = {
  __proto__: new Engine(),

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
