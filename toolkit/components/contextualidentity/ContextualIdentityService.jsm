/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ContextualIdentityService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// The maximum valid numeric value for the userContextId.
const MAX_USER_CONTEXT_ID = -1 >>> 0;
const LAST_CONTAINERS_JSON_VERSION = 4;
const SAVE_DELAY_MS = 1500;
const CONTEXTUAL_IDENTITY_ENABLED_PREF = "privacy.userContext.enabled";

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", function() {
  return new TextDecoder();
});

XPCOMUtils.defineLazyGetter(this, "gTextEncoder", function() {
  return new TextEncoder();
});

ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

function _TabRemovalObserver(resolver, remoteTabIds) {
  this._resolver = resolver;
  this._remoteTabIds = remoteTabIds;
  Services.obs.addObserver(this, "ipc:browser-destroyed");
}

_TabRemovalObserver.prototype = {
  _resolver: null,
  _remoteTabIds: null,

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    let remoteTab = subject.QueryInterface(Ci.nsIRemoteTab);
    if (this._remoteTabIds.has(remoteTab.tabId)) {
      this._remoteTabIds.delete(remoteTab.tabId);
      if (this._remoteTabIds.size == 0) {
        Services.obs.removeObserver(this, "ipc:browser-destroyed");
        this._resolver();
      }
    }
  },
};

function _ContextualIdentityService(path) {
  this.init(path);
}

_ContextualIdentityService.prototype = {
  LAST_CONTAINERS_JSON_VERSION,

  _defaultIdentities: [
    {
      userContextId: 1,
      public: true,
      icon: "fingerprint",
      color: "blue",
      l10nID: "userContextPersonal.label",
      accessKey: "userContextPersonal.accesskey",
      telemetryId: 1,
    },
    {
      userContextId: 2,
      public: true,
      icon: "briefcase",
      color: "orange",
      l10nID: "userContextWork.label",
      accessKey: "userContextWork.accesskey",
      telemetryId: 2,
    },
    {
      userContextId: 3,
      public: true,
      icon: "dollar",
      color: "green",
      l10nID: "userContextBanking.label",
      accessKey: "userContextBanking.accesskey",
      telemetryId: 3,
    },
    {
      userContextId: 4,
      public: true,
      icon: "cart",
      color: "pink",
      l10nID: "userContextShopping.label",
      accessKey: "userContextShopping.accesskey",
      telemetryId: 4,
    },
    {
      userContextId: 5,
      public: false,
      icon: "",
      color: "",
      name: "userContextIdInternal.thumbnail",
      accessKey: "",
    },
    // This userContextId is used by ExtensionStorageIDB.jsm to create an IndexedDB database
    // opened with the extension principal but not directly accessible to the extension code
    // (do not change the userContextId assigned here, otherwise the installed extensions will
    // not be able to access the data previously stored with the browser.storage.local API).
    {
      userContextId: MAX_USER_CONTEXT_ID,
      public: false,
      icon: "",
      color: "",
      name: "userContextIdInternal.webextStorageLocal",
      accessKey: "",
    },
  ],

  _identities: null,
  _openedIdentities: new Set(),
  _lastUserContextId: 0,

  _path: null,
  _dataReady: false,

  _saver: null,

  init(path) {
    this._path = path;

    Services.prefs.addObserver(CONTEXTUAL_IDENTITY_ENABLED_PREF, this);
  },

  async observe(aSubject, aTopic) {
    if (aTopic === "nsPref:changed") {
      const contextualIdentitiesEnabled = Services.prefs.getBoolPref(
        CONTEXTUAL_IDENTITY_ENABLED_PREF
      );
      if (!contextualIdentitiesEnabled) {
        await this.closeContainerTabs();
        this.notifyAllContainersCleared();
        this.resetDefault();
      }
    }
  },

  load() {
    return OS.File.read(this._path).then(
      bytes => {
        // If synchronous loading happened in the meantime, exit now.
        if (this._dataReady) {
          return;
        }

        try {
          this.parseData(bytes);
        } catch (error) {
          this.loadError(error);
        }
      },
      error => {
        this.loadError(error);
      }
    );
  },

  resetDefault() {
    this._identities = [];

    // Compute the last used context id (excluding the reserved userContextIds
    // "userContextIdInternal.webextStorageLocal" which has UINT32_MAX as its
    // userContextId).
    this._lastUserContextId = this._defaultIdentities
      .filter(identity => identity.userContextId < MAX_USER_CONTEXT_ID)
      .map(identity => identity.userContextId)
      .sort((a, b) => a >= b)
      .pop();

    // Clone the array
    for (let identity of this._defaultIdentities) {
      this._identities.push(Object.assign({}, identity));
    }
    this._openedIdentities = new Set();

    this._dataReady = true;

    // Let's delete all the data of any userContextId. 1 is the first valid
    // userContextId value.
    this.deleteContainerData();

    this.saveSoon();
  },

  loadError(error) {
    if (
      error != null &&
      !(error instanceof OS.File.Error && error.becauseNoSuchFile) &&
      !(
        error instanceof Components.Exception &&
        error.result == Cr.NS_ERROR_FILE_NOT_FOUND
      )
    ) {
      // Let's report the error.
      Cu.reportError(error);
    }

    // If synchronous loading happened in the meantime, exit now.
    if (this._dataReady) {
      return;
    }

    this.resetDefault();
  },

  saveSoon() {
    if (!this._saver) {
      this._saverCallback = () => this._saver.finalize();

      this._saver = new DeferredTask(() => this.save(), SAVE_DELAY_MS);
      AsyncShutdown.profileBeforeChange.addBlocker(
        "ContextualIdentityService: writing data",
        this._saverCallback
      );
    } else {
      this._saver.disarm();
    }

    this._saver.arm();
  },

  save() {
    AsyncShutdown.profileBeforeChange.removeBlocker(this._saverCallback);

    this._saver = null;
    this._saverCallback = null;

    let object = {
      version: LAST_CONTAINERS_JSON_VERSION,
      lastUserContextId: this._lastUserContextId,
      identities: this._identities,
    };

    let bytes = gTextEncoder.encode(JSON.stringify(object));
    return OS.File.writeAtomic(this._path, bytes, {
      tmpPath: this._path + ".tmp",
    });
  },

  create(name, icon, color) {
    this.ensureDataReady();

    // Retrieve the next userContextId available.
    let userContextId = ++this._lastUserContextId;

    // Throw an error if the next userContextId available is invalid (the one associated to
    // MAX_USER_CONTEXT_ID is already reserved to "userContextIdInternal.webextStorageLocal", which
    // is also the last valid userContextId available, because its userContextId is equal to UINT32_MAX).
    if (userContextId >= MAX_USER_CONTEXT_ID) {
      throw new Error(
        `Unable to create a new userContext with id '${userContextId}'`
      );
    }

    let identity = {
      userContextId,
      public: true,
      icon,
      color,
      name,
    };

    this._identities.push(identity);
    this.saveSoon();
    Services.obs.notifyObservers(
      this.getIdentityObserverOutput(identity),
      "contextual-identity-created"
    );

    return Cu.cloneInto(identity, {});
  },

  update(userContextId, name, icon, color) {
    this.ensureDataReady();

    let identity = this._identities.find(
      identity => identity.userContextId == userContextId && identity.public
    );
    if (identity && name) {
      identity.name = name;
      identity.color = color;
      identity.icon = icon;
      delete identity.l10nID;
      delete identity.accessKey;

      this.saveSoon();
      Services.obs.notifyObservers(
        this.getIdentityObserverOutput(identity),
        "contextual-identity-updated"
      );
    }

    return !!identity;
  },

  remove(userContextId) {
    this.ensureDataReady();

    let index = this._identities.findIndex(
      i => i.userContextId == userContextId && i.public
    );
    if (index == -1) {
      return false;
    }

    Services.clearData.deleteDataFromOriginAttributesPattern({ userContextId });

    let deletedOutput = this.getIdentityObserverOutput(
      this.getPublicIdentityFromId(userContextId)
    );
    this._identities.splice(index, 1);
    this._openedIdentities.delete(userContextId);
    this.saveSoon();
    Services.obs.notifyObservers(deletedOutput, "contextual-identity-deleted");

    return true;
  },

  getIdentityObserverOutput(identity) {
    let wrappedJSObject = {
      name: this.getUserContextLabel(identity.userContextId),
      icon: identity.icon,
      color: identity.color,
      userContextId: identity.userContextId,
    };

    return { wrappedJSObject };
  },

  parseData(bytes) {
    let data = JSON.parse(gTextDecoder.decode(bytes));
    if (data.version == 1) {
      this.resetDefault();
      return;
    }

    let saveNeeded = false;

    if (data.version == 2) {
      data = this.migrate2to3(data);
      saveNeeded = true;
    }

    if (data.version == 3) {
      data = this.migrate3to4(data);
      saveNeeded = true;
    }

    if (data.version != LAST_CONTAINERS_JSON_VERSION) {
      dump(
        "ERROR - ContextualIdentityService - Unknown version found in " +
          this._path +
          "\n"
      );
      this.loadError(null);
      return;
    }

    this._identities = data.identities;
    this._lastUserContextId = data.lastUserContextId;

    // If we had a migration, let's force the saving of the file.
    if (saveNeeded) {
      this.saveSoon();
    }

    this._dataReady = true;
  },

  ensureDataReady() {
    if (this._dataReady) {
      return;
    }

    try {
      // This reads the file and automatically detects the UTF-8 encoding.
      let inputStream = Cc[
        "@mozilla.org/network/file-input-stream;1"
      ].createInstance(Ci.nsIFileInputStream);
      inputStream.init(
        new FileUtils.File(this._path),
        FileUtils.MODE_RDONLY,
        FileUtils.PERMS_FILE,
        0
      );
      try {
        let bytes = NetUtil.readInputStream(
          inputStream,
          inputStream.available()
        );
        this.parseData(bytes);
      } finally {
        inputStream.close();
      }
    } catch (error) {
      this.loadError(error);
    }
  },

  getPrivateUserContextIds() {
    return this._identities
      .filter(identity => !identity.public)
      .map(identity => identity.userContextId);
  },

  getPublicIdentities() {
    this.ensureDataReady();
    return Cu.cloneInto(
      this._identities.filter(info => info.public),
      {}
    );
  },

  getPrivateIdentity(name) {
    this.ensureDataReady();
    return Cu.cloneInto(
      this._identities.find(info => !info.public && info.name == name),
      {}
    );
  },

  // getDefaultPrivateIdentity is similar to getPrivateIdentity but it only looks in the
  // default identities (e.g. it is used in the data migration methods to retrieve a new default
  // private identity and add it to the containers data stored on file).
  getDefaultPrivateIdentity(name) {
    return Cu.cloneInto(
      this._defaultIdentities.find(info => !info.public && info.name == name),
      {}
    );
  },

  getPublicIdentityFromId(userContextId) {
    this.ensureDataReady();
    return Cu.cloneInto(
      this._identities.find(
        info => info.userContextId == userContextId && info.public
      ),
      {}
    );
  },

  getUserContextLabel(userContextId) {
    let identity = this.getPublicIdentityFromId(userContextId);
    if (!identity) {
      return "";
    }

    // We cannot localize the user-created identity names.
    if (identity.name) {
      return identity.name;
    }

    return gBrowserBundle.GetStringFromName(identity.l10nID);
  },

  setTabStyle(tab) {
    if (!tab.hasAttribute("usercontextid")) {
      return;
    }

    let userContextId = tab.getAttribute("usercontextid");
    let identity = this.getPublicIdentityFromId(userContextId);

    let prefix = "identity-color-";
    /* Remove the existing container color highlight if it exists */
    for (let className of tab.classList) {
      if (className.startsWith(prefix)) {
        tab.classList.remove(className);
      }
    }
    if (identity && identity.color) {
      tab.classList.add(prefix + identity.color);
    }
  },

  countContainerTabs(userContextId = 0) {
    let count = 0;
    this._forEachContainerTab(function() {
      ++count;
    }, userContextId);
    return count;
  },

  closeContainerTabs(userContextId = 0) {
    return new Promise(resolve => {
      let remoteTabIds = new Set();
      this._forEachContainerTab((tab, tabbrowser) => {
        let frameLoader = tab.linkedBrowser.frameLoader;

        // We don't have remoteTab in non-e10s mode.
        if (frameLoader.remoteTab) {
          remoteTabIds.add(frameLoader.remoteTab.tabId);
        }

        tabbrowser.removeTab(tab);
      }, userContextId);

      if (remoteTabIds.size == 0) {
        resolve();
        return;
      }

      new _TabRemovalObserver(resolve, remoteTabIds);
    });
  },

  notifyAllContainersCleared() {
    for (let identity of this._identities) {
      // Don't clear the data related to private identities (e.g. the one used internally
      // for the thumbnails and the one used for the storage.local IndexedDB backend).
      if (!identity.public) {
        continue;
      }
      Services.clearData.deleteDataFromOriginAttributesPattern({
        userContextId: identity.userContextId,
      });
    }
  },

  _forEachContainerTab(callback, userContextId = 0) {
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      if (win.closed || !win.gBrowser) {
        continue;
      }

      let tabbrowser = win.gBrowser;
      for (let i = tabbrowser.tabs.length - 1; i >= 0; --i) {
        let tab = tabbrowser.tabs[i];
        if (
          tab.hasAttribute("usercontextid") &&
          (!userContextId ||
            parseInt(tab.getAttribute("usercontextid"), 10) == userContextId)
        ) {
          callback(tab, tabbrowser);
        }
      }
    }
  },

  telemetry(userContextId) {
    let identity = this.getPublicIdentityFromId(userContextId);

    // Let's ignore unknown identities for now.
    if (!identity) {
      return;
    }

    if (!this._openedIdentities.has(userContextId)) {
      this._openedIdentities.add(userContextId);
      Services.telemetry.getHistogramById("UNIQUE_CONTAINERS_OPENED").add(1);
    }

    Services.telemetry.getHistogramById("TOTAL_CONTAINERS_OPENED").add(1);

    if (identity.telemetryId) {
      Services.telemetry
        .getHistogramById("CONTAINER_USED")
        .add(identity.telemetryId);
    }
  },

  createNewInstanceForTesting(path) {
    return new _ContextualIdentityService(path);
  },

  deleteContainerData() {
    // The userContextId 0 is reserved to the default firefox identity,
    // and it should not be clear when we delete the public containers data.
    let minUserContextId = 1;

    // Collect the userContextId related to the identities that should not be cleared
    // (the ones marked as `public = false`).
    const keepDataContextIds = this.getPrivateUserContextIds();

    // Collect the userContextIds currently used by any stored cookie.
    let cookiesUserContextIds = new Set();

    for (let cookie of Services.cookies.cookies) {
      // Skip any userContextIds that should not be cleared.
      if (
        cookie.originAttributes.userContextId >= minUserContextId &&
        !keepDataContextIds.includes(cookie.originAttributes.userContextId)
      ) {
        cookiesUserContextIds.add(cookie.originAttributes.userContextId);
      }
    }

    for (let userContextId of cookiesUserContextIds) {
      Services.clearData.deleteDataFromOriginAttributesPattern({
        userContextId,
      });
    }
  },

  migrate2to3(data) {
    // migrating from 2 to 3 is basically just increasing the version id.
    // This migration was needed for bug 1419591. See bug 1419591 to know more.
    data.version = 3;

    return data;
  },

  migrate3to4(data) {
    // Migrating from 3 to 4 is:
    // - adding the reserver userContextId used by the webextension storage.local API
    // - add the keepData property to all the existent identities
    // - increasing the version id.
    //
    // This migration was needed for Bug 1406181. See bug 1406181 for rationale.
    const webextStorageLocalIdentity = this.getDefaultPrivateIdentity(
      "userContextIdInternal.webextStorageLocal"
    );

    data.identities.push(webextStorageLocalIdentity);

    data.version = 4;

    return data;
  },
};

let path = OS.Path.join(OS.Constants.Path.profileDir, "containers.json");
var ContextualIdentityService = new _ContextualIdentityService(path);
