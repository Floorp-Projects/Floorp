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

const EXPORTED_SYMBOLS = ['Engines', 'Engine',
                          'FormEngine', 'TabEngine'];

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
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/remote.js");
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

  // "DefaultEngine";
  get logName() { throw "logName property must be overridden in subclasses"; },

  // "user-data/default-engine/";
  get serverPrefix() { throw "serverPrefix property must be overridden in subclasses"; },

  get _remote() {
    if (!this.__remote)
      this.__remote = new RemoteStore(this.serverPrefix, 'Engine:' + this.name);
    return this.__remote;
  },

  get enabled() {
    return Utils.prefs.getBoolPref("engine." + this.name);
  },

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __json: null,
  get _json() {
    if (!this.__json)
      this.__json = Cc["@mozilla.org/dom/json;1"].
        createInstance(Ci.nsIJSON);
    return this.__json;
  },

  // _core, _store and _tracker need to be overridden in subclasses
  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new SyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new Store();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new Tracker();
    return this.__tracker;
  },

  __snapshot: null,
  get _snapshot() {
    if (!this.__snapshot)
      this.__snapshot = new SnapshotStore(this.name);
    return this.__snapshot;
  },
  set _snapshot(value) {
    this.__snapshot = value;
  },

  get _pbeId() {
    let id = ID.get('Engine:PBE:' + this.name);
    if (!id)
      id = ID.get('Engine:PBE:default');
    if (!id)
      throw "No identity found for engine PBE!";
    return id;
  },

  get _engineId() {
    let id = ID.get('Engine:' + this.name)
    if (!id ||
        id.username != this._pbeId.username || id.realm != this._pbeId.realm) {
      let password = null;
      if (id)
        password = id.password;
      id = new Identity(this._pbeId.realm + ' - ' + this.logName,
                        this._pbeId.username, password);
      ID.set('Engine:' + this.name, id);
    }
    return id;
  },

  _init: function Engine__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this.logName);
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.service.engine")];
    this._osPrefix = "weave:" + this.name + ":";
    this._snapshot.load();
  },

  _getSymKey: function Engine__getSymKey() {
    let self = yield;

    if ("none" == Utils.prefs.getCharPref("encryption"))
      return;

    this._remote.keys.get(self.cb);
    yield;
    let keys = this._remote.keys.data;

    if (!keys || !keys.ring || !keys.ring[this._engineId.userHash])
      throw "Keyring does not contain a key for this user";

    Crypto.RSAdecrypt.async(Crypto, self.cb,
                            keys.ring[this._engineId.userHash], this._pbeId);
    let symkey = yield;
    this._engineId.setTempPassword(symkey);

    self.done();
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
    this._log.debug("Resetting server data");
    this._remote.status.delete(self.cb);
    yield;
    this._remote.keys.delete(self.cb);
    yield;
    this._remote.snapshot.delete(self.cb);
    yield;
    this._remote.deltas.delete(self.cb);
    yield;
    this._log.debug("Server files deleted");
  },

  _resetClient: function Engine__resetClient() {
    let self = yield;
    this._log.debug("Resetting client state");
    this._snapshot.wipe();
    this._store.wipe();
    this._log.debug("Client reset completed successfully");
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

  _sync: function Engine__sync() {
    let self = yield;

    this._log.info("Beginning sync");

    // Before we get started, make sure we have a remote directory to play in
    DAV.MKCOL(this.serverPrefix, self.cb);
    let ret = yield;
    if (!ret)
      throw "Could not create remote folder";

    // 1) Fetch server deltas
    this._getServerData.async(this, self.cb);
    let server = yield;

    this._log.info("Local snapshot version: " + this._snapshot.version);
    this._log.info("Server status: " + server.status);
    this._log.info("Server maxVersion: " + server.maxVersion);
    this._log.info("Server snapVersion: " + server.snapVersion);

    if (server.status != 0) {
      this._log.fatal("Sync error: could not get server status, " +
                      "or initial upload failed.  Aborting sync.");
      return;
    }

    // 2) Generate local deltas from snapshot -> current client status

    let localJson = new SnapshotStore();
    localJson.data = this._store.wrap();
    this._core.detectUpdates(self.cb, this._snapshot.data, localJson.data);
    let localUpdates = yield;

    this._log.trace("local json:\n" + localJson.serialize());
    this._log.trace("Local updates: " + this._serializeCommands(localUpdates));
    this._log.trace("Server updates: " + this._serializeCommands(server.updates));

    if (server.updates.length == 0 && localUpdates.length == 0) {
      this._snapshot.version = server.maxVersion;
      this._log.info("Sync complete: no changes needed on client or server");
      self.done(true);
      return;
    }

    // 3) Reconcile client/server deltas and generate new deltas for them.

    this._log.info("Reconciling client/server updates");
    this._core.reconcile(self.cb, localUpdates, server.updates);
    ret = yield;

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
      this._log.info("Sync complete: no changes needed on client or server");
      this._snapshot.data = localJson.data;
      this._snapshot.version = server.maxVersion;
      this._snapshot.save();
      self.done(true);
      return;
    }

    if (clientConflicts.length || serverConflicts.length) {
      this._log.warn("Conflicts found!  Discarding server changes");
    }

    let savedSnap = Utils.deepCopy(this._snapshot.data);
    let savedVersion = this._snapshot.version;
    let newSnapshot;

    // 3.1) Apply server changes to local store
    if (clientChanges.length) {
      this._log.info("Applying changes locally");
      // Note that we need to need to apply client changes to the
      // current tree, not the saved snapshot

      localJson.applyCommands.async(localJson, self.cb, clientChanges);
      yield;
      this._snapshot.data = localJson.data;
      this._snapshot.version = server.maxVersion;
      this._store.applyCommands.async(this._store, self.cb, clientChanges);
      yield;
      newSnapshot = this._store.wrap();

      this._core.detectUpdates(self.cb, this._snapshot.data, newSnapshot);
      let diff = yield;
      if (diff.length != 0) {
        this._log.warn("Commands did not apply correctly");
        this._log.debug("Diff from snapshot+commands -> " +
                        "new snapshot after commands:\n" +
                        this._serializeCommands(diff));
        // FIXME: do we really want to revert the snapshot here?
        this._snapshot.data = Utils.deepCopy(savedSnap);
        this._snapshot.version = savedVersion;
      }
      this._snapshot.save();
    }

    // 3.2) Append server delta to the delta file and upload

    // Generate a new diff, from the current server snapshot to the
    // current client snapshot.  In the case where there are no
    // conflicts, it should be the same as what the resolver returned

    newSnapshot = this._store.wrap();
    this._core.detectUpdates(self.cb, server.snapshot, newSnapshot);
    let serverDelta = yield;

    // Log an error if not the same
    if (!(serverConflicts.length ||
          Utils.deepEquals(serverChanges, serverDelta)))
      this._log.warn("Predicted server changes differ from " +
                     "actual server->client diff (can be ignored in many cases)");

    this._log.info("Actual changes for server: " + serverDelta.length);
    this._log.debug("Actual changes for server: " +
                    this._serializeCommands(serverDelta));

    if (serverDelta.length) {
      this._log.info("Uploading changes to server");

      this._snapshot.data = newSnapshot;
      this._snapshot.version = ++server.maxVersion;

      server.deltas.push(serverDelta);

      if (server.formatVersion != ENGINE_STORAGE_FORMAT_VERSION ||
          this._encryptionChanged) {
        this._fullUpload.async(this, self.cb);
        let status = yield;
        if (!status)
          this._log.error("Could not upload files to server"); // eep?

      } else {
        this._remote.deltas.put(self.cb, this._serializeCommands(server.deltas));
        yield;

        let c = 0;
        for (GUID in this._snapshot.data)
          c++;

        this._remote.status.put(self.cb,
                                {GUID: this._snapshot.GUID,
                                 formatVersion: ENGINE_STORAGE_FORMAT_VERSION,
                                 snapVersion: server.snapVersion,
                                 maxVersion: this._snapshot.version,
                                 snapEncryption: server.snapEncryption,
                                 deltasEncryption: Crypto.defaultAlgorithm,
                                 itemCount: c});

        this._log.info("Successfully updated deltas and status on server");
        this._snapshot.save();
      }
    }

    this._log.info("Sync complete");
    self.done(true);
  },

  /* Get the deltas/combined updates from the server
   * Returns:
   *   status:
   *     -1: error
   *      0: ok
   * These fields may be null when status is -1:
   *   formatVersion:
   *     version of the data format itself.  For compatibility checks.
   *   maxVersion:
   *     the latest version on the server
   *   snapVersion:
   *     the version of the current snapshot on the server (deltas not applied)
   *   snapEncryption:
   *     encryption algorithm currently used on the server-stored snapshot
   *   deltasEncryption:
   *     encryption algorithm currently used on the server-stored deltas
   *   snapshot:
   *     full snapshot of the latest server version (deltas applied)
   *   deltas:
   *     all of the individual deltas on the server
   *   updates:
   *     the relevant deltas (from our snapshot version to current),
   *     combined into a single set.
   */
  _getServerData: function BmkEngine__getServerData() {
    let self = yield;
    let status;

    try {
      this._log.debug("Getting status file from server");
      this._remote.status.get(self.cb);
      status = yield;
      this._log.info("Got status file from server");

    } catch (e if e.message.status == 404) {
      this._log.info("Server has no status file, Initial upload to server");

      this._snapshot.data = this._store.wrap();
      this._snapshot.version = 0;
      this._snapshot.GUID = null; // in case there are other snapshots out there

      this._fullUpload.async(this, self.cb);
      let uploadStatus = yield;
      if (!uploadStatus)
        throw "Initial upload failed";

      this._log.info("Initial upload to server successful");
      this._snapshot.save();

      self.done({status: 0,
                 formatVersion: ENGINE_STORAGE_FORMAT_VERSION,
                 maxVersion: this._snapshot.version,
                 snapVersion: this._snapshot.version,
                 snapEncryption: Crypto.defaultAlgorithm,
                 deltasEncryption: Crypto.defaultAlgorithm,
                 snapshot: Utils.deepCopy(this._snapshot.data),
                 deltas: [],
                 updates: []});
      return;
    }

    let ret = {status: -1,
               formatVersion: null, maxVersion: null, snapVersion: null,
               snapEncryption: null, deltasEncryption: null,
               snapshot: null, deltas: null, updates: null};
    let deltas, allDeltas;
    let snap = new SnapshotStore();

    // Bail out if the server has a newer format version than we can parse
    if (status.formatVersion > ENGINE_STORAGE_FORMAT_VERSION) {
      this._log.error("Server uses storage format v" + status.formatVersion +
                      ", this client understands up to v" + ENGINE_STORAGE_FORMAT_VERSION);
      throw "Incompatible server format for engine data";
    }

    this._getSymKey.async(this, self.cb);
    yield;

    if (status.formatVersion == 0) {
      ret.snapEncryption = status.snapEncryption = "none";
      ret.deltasEncryption = status.deltasEncryption = "none";
    }

    if (status.GUID != this._snapshot.GUID) {
      this._log.info("Remote/local sync GUIDs do not match.  " +
                     "Forcing initial sync.");
      this._log.debug("Remote: " + status.GUID);
      this._log.debug("Local: " + this._snapshot.GUID);
      this._store.resetGUIDs();
      this._snapshot.data = {};
      this._snapshot.version = -1;
      this._snapshot.GUID = status.GUID;
    }

    if (this._snapshot.version < status.snapVersion) {
      this._log.trace("Local snapshot version < server snapVersion");

      if (this._snapshot.version >= 0)
        this._log.info("Local snapshot is out of date");

      this._log.info("Downloading server snapshot");
      this._remote.snapshot.get(self.cb); // fixme: doesn't use status.snapEncryption
      snap.data = yield;

      this._log.info("Downloading server deltas");
      this._remote.deltas.get(self.cb); // fixme: doesn't use status.deltasEncryption
      allDeltas = yield;
      deltas = allDeltas;

    } else if (this._snapshot.version >= status.snapVersion &&
               this._snapshot.version < status.maxVersion) {
      this._log.trace("Server snapVersion <= local snapshot version < server maxVersion");
      snap.data = Utils.deepCopy(this._snapshot.data);

      this._log.info("Downloading server deltas");
      this._remote.deltas.get(self.cb); // fixme: doesn't use status.deltasEncryption
      allDeltas = yield;
      deltas = allDeltas.slice(this._snapshot.version - status.snapVersion);

    } else if (this._snapshot.version == status.maxVersion) {
      this._log.trace("Local snapshot version == server maxVersion");
      snap.data = Utils.deepCopy(this._snapshot.data);

      // FIXME: could optimize this case by caching deltas file
      this._log.info("Downloading server deltas");
      this._remote.deltas.get(self.cb); // fixme: doesn't use status.deltasEncryption
      allDeltas = yield;
      deltas = [];

    } else { // this._snapshot.version > status.maxVersion
      this._log.error("Server snapshot is older than local snapshot");
      throw "Server snapshot is older than local snapshot";
    }

    try {
      for (var i = 0; i < deltas.length; i++) {
        snap.applyCommands.async(snap, self.cb, deltas[i]);
        yield;
      }
    } catch (e) {
      this._log.error("Error applying remote deltas to saved snapshot");
      this._log.error("Clearing local snapshot; next sync will merge");
      this._log.debug("Exception: " + Utils.exceptionStr(e));
      this._log.trace("Stack:\n" + Utils.stackTrace(e));
      this._snapshot.wipe();
      throw e;
    }

    ret.status = 0;
    ret.formatVersion = status.formatVersion;
    ret.maxVersion = status.maxVersion;
    ret.snapVersion = status.snapVersion;
    ret.snapEncryption = status.snapEncryption;
    ret.deltasEncryption = status.deltasEncryption;
    ret.snapshot = snap.data;
    ret.deltas = allDeltas;
    this._core.detectUpdates(self.cb, this._snapshot.data, snap.data);
    ret.updates = yield;

    self.done(ret)
  },

  _fullUpload: function Engine__fullUpload() {
    let self = yield;
    let ret = false;

    Crypto.PBEkeygen.async(Crypto, self.cb);
    let symkey = yield;
    this._engineId.setTempPassword(symkey);
    if (!this._engineId.password)
      throw "Could not generate a symmetric encryption key";

    let enckey = this._engineId.password;
    if ("none" != Utils.prefs.getCharPref("encryption")) {
      Crypto.RSAencrypt.async(Crypto, self.cb,
                              this._engineId.password, this._pbeId);
      enckey = yield;
    }

    if (!enckey)
      throw "Could not encrypt symmetric encryption key";

    let keys = {ring: {}};
    keys.ring[this._engineId.userHash] = enckey;
    this._remote.keys.put(self.cb, keys);
    yield;

    this._remote.snapshot.put(self.cb, this._snapshot.wrap());
    yield;
    this._remote.deltas.put(self.cb, []);
    yield;

    let c = 0;
    for (GUID in this._snapshot.data)
      c++;

    this._remote.status.put(self.cb,
                            {GUID: this._snapshot.GUID,
                             formatVersion: ENGINE_STORAGE_FORMAT_VERSION,
                             snapVersion: this._snapshot.version,
                             maxVersion: this._snapshot.version,
                             snapEncryption: Crypto.defaultAlgorithm,
                             deltasEncryption: "none",
                             itemCount: c});
    yield;

    this._log.info("Full upload to server successful");
    ret = true;
    self.done(ret);
  },

  _share: function Engine__share(username) {
    let self = yield;
    let prefix = DAV.defaultPrefix;

    this._log.debug("Sharing bookmarks with " + username);

    this._getSymKey.async(this, self.cb);
    yield;

    // copied from getSymKey
    DAV.GET(this.keysFile, self.cb);
    let ret = yield;
    Utils.ensureStatus(ret.status, "Could not get keys file.");
    let keys = this._json.decode(ret.responseText);

    // get the other user's pubkey
    let hash = Utils.sha1(username);
    let serverURL = Utils.prefs.getCharPref("serverURL");

    try {
      DAV.defaultPrefix = "user/" + hash + "/";  //FIXME: very ugly!
      DAV.GET("public/pubkey", self.cb);
      ret = yield;
    }
    catch (e) { throw e; }
    finally { DAV.defaultPrefix = prefix; }

    Utils.ensureStatus(ret.status, "Could not get public key for " + username);

    let id = new Identity();
    id.pubkey = ret.responseText;

    // now encrypt the symkey with their pubkey and upload the new keyring
    Crypto.RSAencrypt.async(Crypto, self.cb, this._engineId.password, id);
    let enckey = yield;
    if (!enckey)
      throw "Could not encrypt symmetric encryption key";

    keys.ring[hash] = enckey;
    DAV.PUT(this.keysFile, this._json.encode(keys), self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload keyring file.");

    this._createShare(username, username);

    this._log.debug("All done sharing!");

    self.done(true);
  },

  // FIXME: EEK bookmarks specific
  _createShare: function Engine__createShare(id, title) {
    let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);
    let ans = Cc["@mozilla.org/browser/annotation-service;1"].
      getService(Ci.nsIAnnotationService);

    let root;
    let a = ans.getItemsWithAnnotation("weave/mounted-shares-folder", {});
    if (a.length == 1)
      root = a[0];

    if (!root) {
      root = bms.createFolder(bms.toolbarFolder, "Shared Folders",
                              bms.DEFAULT_INDEX);
      ans.setItemAnnotation(root, "weave/mounted-shares-folder", true, 0,
                            ans.EXPIRE_NEVER);
    }

    let item;
    a = ans.getItemsWithAnnotation("weave/mounted-share-id", {});
    for (let i = 0; i < a.length; i++) {
      if (ans.getItemAnnotation(a[i], "weave/mounted-share-id") == id) {
        item = a[i];
        break;
      }
    }

    if (!item) {
      let newId = bms.createFolder(root, title, bms.DEFAULT_INDEX);
      ans.setItemAnnotation(newId, "weave/mounted-share-id", id, 0,
                            ans.EXPIRE_NEVER);
    }
  },

  sync: function Engine_sync(onComplete) {
    return this._sync.async(this, onComplete);
  },

  share: function Engine_share(onComplete, username) {
    return this._share.async(this, onComplete, username);
  },

  resetServer: function Engine_resetServer(onComplete) {
    this._notify("reset-server", this._resetServer).async(this, onComplete);
  },

  resetClient: function Engine_resetClient(onComplete) {
    this._notify("reset-client", this._resetClient).async(this, onComplete);
  }
};

function FormEngine(pbeId) {
  this._init(pbeId);
}
FormEngine.prototype = {
  get name() { return "forms"; },
  get logName() { return "FormEngine"; },
  get serverPrefix() { return "user-data/forms/"; },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new FormSyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new FormStore();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new FormsTracker();
    return this.__tracker;
  }
};
FormEngine.prototype.__proto__ = new Engine();

function TabEngine(pbeId) {
  this._init(pbeId);
}
TabEngine.prototype = {
  __proto__: new Engine(),

  get name() "tabs",
  get logName() "TabEngine",
  get serverPrefix() "user-data/tabs/",
  get store() this._store,

  get _core() {
    let core = new TabSyncCore(this);
    this.__defineGetter__("_core", function() core);
    return this._core;
  },

  get _store() {
    let store = new TabStore();
    this.__defineGetter__("_store", function() store);
    return this._store;
  },

  get _tracker() {
    let tracker = new TabTracker(this);
    this.__defineGetter__("_tracker", function() tracker);
    return this._tracker;
  }

};
