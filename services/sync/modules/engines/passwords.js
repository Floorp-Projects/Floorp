const EXPORTED_SYMBOLS = ['PasswordEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");

function PasswordEngine(pbeId) {
  this._init(pbeId);
}
PasswordEngine.prototype = {
  get name() { return "passwords"; },
  get logName() { return "PasswordEngine"; },
  get serverPrefix() { return "user-data/passwords/"; },

  __core: null,
  get _core() {
    if (!this.__core) {
      this.__core = new PasswordSyncCore();
      this.__core._hashLoginInfo = this._hashLoginInfo;
    }
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store) {
      this.__store = new PasswordStore();
      this.__store._hashLoginInfo = this._hashLoginInfo;
    }
    return this.__store;
  },

  /*
   * _hashLoginInfo
   *
   * nsILoginInfo objects don't have a unique GUID, so we need to generate one
   * on the fly. This is done by taking a hash of every field in the object.
   * Note that the resulting GUID could potentiually reveal passwords via
   * dictionary attacks or brute force. But GUIDs shouldn't be obtainable by
   * anyone, so this should generally be safe.
   */
  _hashLoginInfo : function (aLogin) {
    var loginKey = aLogin.hostname      + ":" +
                   aLogin.formSubmitURL + ":" +
                   aLogin.httpRealm     + ":" +
                   aLogin.username      + ":" +
                   aLogin.password      + ":" +
                   aLogin.usernameField + ":" +
                   aLogin.passwordField;

    return Utils.sha1(loginKey);
  }
};
PasswordEngine.prototype.__proto__ = new Engine();

function PasswordSyncCore() {
  this._init();
}
PasswordSyncCore.prototype = {
  _logName: "PasswordSync",

  __loginManager : null,
  get _loginManager() {
    if (!this.__loginManager)
      this.__loginManager = Cc["@mozilla.org/login-manager;1"].
                            getService(Ci.nsILoginManager);
    return this.__loginManager;
  },

  _itemExists: function PSC__itemExists(GUID) {
    var found = false;
    var logins = this._loginManager.getAllLogins({});

    // XXX It would be more efficient to compute all the hashes in one shot,
    // cache the results, and check the cache here. That would need to happen
    // once per sync -- not sure how to invalidate cache after current sync?
    for (var i = 0; i < logins.length && !found; i++) {
        var hash = this._hashLoginInfo(logins[i]);
        if (hash == GUID)
            found = true;;
    }

    return found;
  },

  _commandLike: function PSC_commandLike(a, b) {
    // Not used.
    return false;
  }
};
PasswordSyncCore.prototype.__proto__ = new SyncCore();

function PasswordStore() {
  this._init();
}
PasswordStore.prototype = {
  _logName: "PasswordStore",

  __loginManager : null,
  get _loginManager() {
    if (!this.__loginManager)
      this.__loginManager = Cc["@mozilla.org/login-manager;1"].
                            getService(Ci.nsILoginManager);
    return this.__loginManager;
  },

  __nsLoginInfo : null,
  get _nsLoginInfo() {
    if (!this.__nsLoginInfo)
      this.__nsLoginInfo = new Components.Constructor(
            "@mozilla.org/login-manager/loginInfo;1",
            Ci.nsILoginInfo, "init");
    return this.__nsLoginInfo;
  },


  _createCommand: function PasswordStore__createCommand(command) {
    this._log.info("PasswordStore got createCommand: " + command );

    var login = new this._nsLoginInfo(command.data.hostname,
                                      command.data.formSubmitURL,
                                      command.data.httpRealm,
                                      command.data.username,
                                      command.data.password,
                                      command.data.usernameField,
                                      command.data.passwordField);

    this._loginManager.addLogin(login);
  },

  _removeCommand: function PasswordStore__removeCommand(command) {
    this._log.info("PasswordStore got removeCommand: " + command );

    var login = new this._nsLoginInfo(command.data.hostname,
                                      command.data.formSubmitURL,
                                      command.data.httpRealm,
                                      command.data.username,
                                      command.data.password,
                                      command.data.usernameField,
                                      command.data.passwordField);

    this._loginManager.removeLogin(login);
  },

  _editCommand: function PasswordStore__editCommand(command) {
    this._log.info("PasswordStore got editCommand: " + command );
    throw "Password syncs are expected to only be create/remove!";
  },

  wrap: function PasswordStore_wrap() {
    /* Return contents of this store, as JSON. */
    var items = {};

    var logins = this._loginManager.getAllLogins({});

    for (var i = 0; i < logins.length; i++) {
      var login = logins[i];

      var key = this._hashLoginInfo(login);

      items[key] = { hostname      : login.hostname,
                     formSubmitURL : login.formSubmitURL,
                     httpRealm     : login.httpRealm,
                     username      : login.username,
                     password      : login.password,
                     usernameField : login.usernameField,
                     passwordField : login.passwordField };
    }

    return items;
  },

  wipe: function PasswordStore_wipe() {
    this._loginManager.removeAllLogins();
  },

  resetGUIDs: function PasswordStore_resetGUIDs() {
    // Not needed.
  }
};
PasswordStore.prototype.__proto__ = new Store();

