/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["SocialService"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

const URI_EXTENSION_STRINGS  = "chrome://mozapps/locale/extensions/extensions.properties";
const ADDON_TYPE_SERVICE     = "service";
const ID_SUFFIX              = "@services.mozilla.org";
const STRING_TYPE_NAME       = "type.%ID%.name";

XPCOMUtils.defineLazyModuleGetter(this, "getFrameWorkerHandle", "resource://gre/modules/FrameWorker.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WorkerAPI", "resource://gre/modules/WorkerAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozSocialAPI", "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "closeAllChatWindows", "resource://gre/modules/MozSocialAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredTask", "resource://gre/modules/DeferredTask.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "etld",
                                   "@mozilla.org/network/effective-tld-service;1",
                                   "nsIEffectiveTLDService");

/**
 * The SocialService is the public API to social providers - it tracks which
 * providers are installed and enabled, and is the entry-point for access to
 * the provider itself.
 */

// Internal helper methods and state
let SocialServiceInternal = {
  get enabled() this.providerArray.length > 0,

  get providerArray() {
    return [p for ([, p] of Iterator(this.providers))];
  },
  get manifests() {
    // Retrieve the manifests of installed providers from prefs
    let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
    let prefs = MANIFEST_PREFS.getChildList("", []);
    for (let pref of prefs) {
      // we only consider manifests in user level prefs to be *installed*
      if (!MANIFEST_PREFS.prefHasUserValue(pref))
        continue;
      try {
        var manifest = JSON.parse(MANIFEST_PREFS.getComplexValue(pref, Ci.nsISupportsString).data);
        if (manifest && typeof(manifest) == "object" && manifest.origin)
          yield manifest;
      } catch (err) {
        Cu.reportError("SocialService: failed to load manifest: " + pref +
                       ", exception: " + err);
      }
    }
  },
  getManifestPrefname: function(origin) {
    // Retrieve the prefname for a given origin/manifest.
    // If no existing pref, return a generated prefname.
    let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
    let prefs = MANIFEST_PREFS.getChildList("", []);
    for (let pref of prefs) {
      try {
        var manifest = JSON.parse(MANIFEST_PREFS.getComplexValue(pref, Ci.nsISupportsString).data);
        if (manifest.origin == origin) {
          return pref;
        }
      } catch (err) {
        Cu.reportError("SocialService: failed to load manifest: " + pref +
                       ", exception: " + err);
      }
    }
    let originUri = Services.io.newURI(origin, null, null);
    return originUri.hostPort.replace('.','-');
  },
  orderedProviders: function(aCallback) {
    if (SocialServiceInternal.providerArray.length < 2) {
      schedule(function () {
        aCallback(SocialServiceInternal.providerArray);
      });
      return;
    }
    // query moz_hosts for frecency.  since some providers may not have a
    // frecency entry, we need to later sort on our own. We use the providers
    // object below as an easy way to later record the frecency on the provider
    // object from the query results.
    let hosts = [];
    let providers = {};

    for (let p of SocialServiceInternal.providerArray) {
      p.frecency = 0;
      providers[p.domain] = p;
      hosts.push(p.domain);
    };

    // cannot bind an array to stmt.params so we have to build the string
    let stmt = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                 .DBConnection.createAsyncStatement(
      "SELECT host, frecency FROM moz_hosts WHERE host IN (" +
      [ '"' + host + '"' for each (host in hosts) ].join(",") + ") "
    );

    try {
      stmt.executeAsync({
        handleResult: function(aResultSet) {
          let row;
          while ((row = aResultSet.getNextRow())) {
            let rh = row.getResultByName("host");
            let frecency = row.getResultByName("frecency");
            providers[rh].frecency = parseInt(frecency) || 0;
          }
        },
        handleError: function(aError) {
          Cu.reportError(aError.message + " (Result = " + aError.result + ")");
        },
        handleCompletion: function(aReason) {
          // the query may not have returned all our providers, so we have
          // stamped the frecency on the provider and sort here. This makes sure
          // all enabled providers get sorted even with frecency zero.
          let providerList = SocialServiceInternal.providerArray;
          // reverse sort
          aCallback(providerList.sort(function(a, b) b.frecency - a.frecency));
        }
      });
    } finally {
      stmt.finalize();
    }
  }
};

XPCOMUtils.defineLazyGetter(SocialServiceInternal, "providers", function () {
  initService();
  let providers = {};
  for (let manifest of this.manifests) {
    try {
      if (ActiveProviders.has(manifest.origin)) {
        // enable the api when a provider is enabled
        MozSocialAPI.enabled = true;
        let provider = new SocialProvider(manifest);
        providers[provider.origin] = provider;
      }
    } catch (err) {
      Cu.reportError("SocialService: failed to load provider: " + manifest.origin +
                     ", exception: " + err);
    }
  }
  return providers;
});

function getOriginActivationType(origin) {
  // if this is an about uri, treat it as a directory
  let URI = Services.io.newURI(origin, null, null);
  let principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(URI);
  if (Services.scriptSecurityManager.isSystemPrincipal(principal) || origin == "moz-safe-about:home") {
    return "internal";
  }

  let directories = Services.prefs.getCharPref("social.directories").split(',');
  if (directories.indexOf(origin) >= 0)
    return "directory";

  return "foreign";
}

let ActiveProviders = {
  get _providers() {
    delete this._providers;
    this._providers = {};
    try {
      let pref = Services.prefs.getComplexValue("social.activeProviders",
                                                Ci.nsISupportsString);
      this._providers = JSON.parse(pref);
    } catch(ex) {}
    return this._providers;
  },

  has: function (origin) {
    return (origin in this._providers);
  },

  add: function (origin) {
    this._providers[origin] = 1;
    this._deferredTask.arm();
  },

  delete: function (origin) {
    delete this._providers[origin];
    this._deferredTask.arm();
  },

  flush: function () {
    this._deferredTask.disarm();
    this._persist();
  },

  get _deferredTask() {
    delete this._deferredTask;
    return this._deferredTask = new DeferredTask(this._persist.bind(this), 0);
  },

  _persist: function () {
    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(this._providers);
    Services.prefs.setComplexValue("social.activeProviders",
                                   Ci.nsISupportsString, string);
  }
};

function migrateSettings() {
  let activeProviders, enabled;
  try {
    activeProviders = Services.prefs.getCharPref("social.activeProviders");
  } catch(e) {
    // not set, we'll check if we need to migrate older prefs
  }
  if (Services.prefs.prefHasUserValue("social.enabled")) {
    enabled = Services.prefs.getBoolPref("social.enabled");
  }
  if (activeProviders) {
    // migration from fx21 to fx22 or later
    // ensure any *builtin* provider in activeproviders is in user level prefs
    for (let origin in ActiveProviders._providers) {
      let prefname;
      let manifest;
      let defaultManifest;
      try {
        prefname = getPrefnameFromOrigin(origin);
        manifest = JSON.parse(Services.prefs.getComplexValue(prefname, Ci.nsISupportsString).data);
      } catch(e) {
        // Our preference is missing or bad, remove from ActiveProviders and
        // continue. This is primarily an error-case and should only be
        // reached by either messing with preferences or hitting the one or
        // two days of nightly that ran into it, so we'll flush right away.
        ActiveProviders.delete(origin);
        ActiveProviders.flush();
        continue;
      }
      let needsUpdate = !manifest.updateDate;
      // fx23 may have built-ins with shareURL
      try {
        defaultManifest = Services.prefs.getDefaultBranch(null)
                        .getComplexValue(prefname, Ci.nsISupportsString).data;
        defaultManifest = JSON.parse(defaultManifest);
      } catch(e) {
        // not a built-in, continue
      }
      if (defaultManifest) {
        if (defaultManifest.shareURL && !manifest.shareURL) {
          manifest.shareURL = defaultManifest.shareURL;
          needsUpdate = true;
        }
        if (defaultManifest.version && (!manifest.version || defaultManifest.version > manifest.version)) {
          manifest = defaultManifest;
          needsUpdate = true;
        }
      }
      if (needsUpdate) {
        // the provider was installed with an older build, so we will update the
        // timestamp and ensure the manifest is in user prefs
        delete manifest.builtin;
        // we're potentially updating for share, so always mark the updateDate
        manifest.updateDate = Date.now();
        if (!manifest.installDate)
          manifest.installDate = 0; // we don't know when it was installed

        let string = Cc["@mozilla.org/supports-string;1"].
                     createInstance(Ci.nsISupportsString);
        string.data = JSON.stringify(manifest);
        Services.prefs.setComplexValue(prefname, Ci.nsISupportsString, string);
      }
      // as of fx 29, we no longer rely on social.enabled. migration from prior
      // versions should disable all service addons if social.enabled=false
      if (enabled === false) {
        ActiveProviders.delete(origin);
      }
    }
    ActiveProviders.flush();
    Services.prefs.clearUserPref("social.enabled");
    return;
  }

  // primary migration from pre-fx21
  let active;
  try {
    active = Services.prefs.getBoolPref("social.active");
  } catch(e) {}
  if (!active)
    return;

  // primary difference from SocialServiceInternal.manifests is that we
  // only read the default branch here.
  let manifestPrefs = Services.prefs.getDefaultBranch("social.manifest.");
  let prefs = manifestPrefs.getChildList("", []);
  for (let pref of prefs) {
    try {
      let manifest;
      try {
        manifest = JSON.parse(manifestPrefs.getComplexValue(pref, Ci.nsISupportsString).data);
      } catch(e) {
        // bad or missing preference, we wont update this one.
        continue;
      }
      if (manifest && typeof(manifest) == "object" && manifest.origin) {
        // our default manifests have been updated with the builtin flags as of
        // fx22, delete it so we can set the user-pref
        delete manifest.builtin;
        if (!manifest.updateDate) {
          manifest.updateDate = Date.now();
          manifest.installDate = 0; // we don't know when it was installed
        }

        let string = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
        string.data = JSON.stringify(manifest);
        // pref here is just the branch name, set the full pref name
        Services.prefs.setComplexValue("social.manifest." + pref, Ci.nsISupportsString, string);
        ActiveProviders.add(manifest.origin);
        ActiveProviders.flush();
        // social.active was used at a time that there was only one
        // builtin, we'll assume that is still the case
        return;
      }
    } catch (err) {
      Cu.reportError("SocialService: failed to load manifest: " + pref + ", exception: " + err);
    }
  }
}

function initService() {
  Services.obs.addObserver(function xpcomShutdown() {
    ActiveProviders.flush();
    SocialService._providerListeners = null;
    Services.obs.removeObserver(xpcomShutdown, "xpcom-shutdown");
  }, "xpcom-shutdown", false);

  try {
    migrateSettings();
  } catch(e) {
    // no matter what, if migration fails we do not want to render social
    // unusable. Worst case scenario is that, when upgrading Firefox, previously
    // enabled providers are not migrated.
    Cu.reportError("Error migrating social settings: " + e);
  }
}

function schedule(callback) {
  Services.tm.mainThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
}

// Public API
this.SocialService = {
  get hasEnabledProviders() {
    // used as an optimization during startup, can be used to check if further
    // initialization should be done (e.g. creating the instances of
    // SocialProvider and turning on UI). ActiveProviders may have changed and
    // not yet flushed so we check the active providers array
    for (let p in ActiveProviders._providers) {
      return true;
    };
    return false;
  },
  get enabled() {
    return SocialServiceInternal.enabled;
  },
  set enabled(val) {
    throw new Error("not allowed to set SocialService.enabled");
  },

  // Enables a provider, the manifest must already exist in prefs. The provider
  // may or may not have previously been added. onDone is always called
  // - with null if no such provider exists, or the activated provider on
  // success.
  enableProvider: function enableProvider(origin, onDone) {
    if (SocialServiceInternal.providers[origin]) {
      schedule(function() {
        onDone(SocialServiceInternal.providers[origin]);
      });
      return;
    }
    let manifest = SocialService.getManifestByOrigin(origin);
    if (manifest) {
      let addon = new AddonWrapper(manifest);
      AddonManagerPrivate.callAddonListeners("onEnabling", addon, false);
      addon.pendingOperations |= AddonManager.PENDING_ENABLE;
      this.addProvider(manifest, onDone);
      addon.pendingOperations -= AddonManager.PENDING_ENABLE;
      AddonManagerPrivate.callAddonListeners("onEnabled", addon);
      return;
    }
    schedule(function() {
      onDone(null);
    });
  },

  // Adds a provider given a manifest, and returns the added provider.
  addProvider: function addProvider(manifest, onDone) {
    if (SocialServiceInternal.providers[manifest.origin])
      throw new Error("SocialService.addProvider: provider with this origin already exists");

    // enable the api when a provider is enabled
    MozSocialAPI.enabled = true;
    let provider = new SocialProvider(manifest);
    SocialServiceInternal.providers[provider.origin] = provider;
    ActiveProviders.add(provider.origin);

    this.getOrderedProviderList(function (providers) {
      this._notifyProviderListeners("provider-enabled", provider.origin, providers);
      if (onDone)
        onDone(provider);
    }.bind(this));
  },

  // Removes a provider with the given origin, and notifies when the removal is
  // complete.
  disableProvider: function disableProvider(origin, onDone) {
    if (!(origin in SocialServiceInternal.providers))
      throw new Error("SocialService.disableProvider: no provider with origin " + origin + " exists!");

    let provider = SocialServiceInternal.providers[origin];
    let manifest = SocialService.getManifestByOrigin(origin);
    let addon = manifest && new AddonWrapper(manifest);
    if (addon) {
      AddonManagerPrivate.callAddonListeners("onDisabling", addon, false);
      addon.pendingOperations |= AddonManager.PENDING_DISABLE;
    }
    provider.enabled = false;

    ActiveProviders.delete(provider.origin);

    delete SocialServiceInternal.providers[origin];
    // disable the api if we have no enabled providers
    MozSocialAPI.enabled = SocialServiceInternal.enabled;

    if (addon) {
      // we have to do this now so the addon manager ui will update an uninstall
      // correctly.
      addon.pendingOperations -= AddonManager.PENDING_DISABLE;
      AddonManagerPrivate.callAddonListeners("onDisabled", addon);
      AddonManagerPrivate.notifyAddonChanged(addon.id, ADDON_TYPE_SERVICE, false);
    }

    this.getOrderedProviderList(function (providers) {
      this._notifyProviderListeners("provider-disabled", origin, providers);
      if (onDone)
        onDone();
    }.bind(this));
  },

  // Returns a single provider object with the specified origin.  The provider
  // must be "installed" (ie, in ActiveProviders)
  getProvider: function getProvider(origin, onDone) {
    schedule((function () {
      onDone(SocialServiceInternal.providers[origin] || null);
    }).bind(this));
  },

  // Returns an unordered array of installed providers
  getProviderList: function(onDone) {
    schedule(function () {
      onDone(SocialServiceInternal.providerArray);
    });
  },

  getManifestByOrigin: function(origin) {
    for (let manifest of SocialServiceInternal.manifests) {
      if (origin == manifest.origin) {
        return manifest;
      }
    }
    return null;
  },

  // Returns an array of installed providers, sorted by frecency
  getOrderedProviderList: function(onDone) {
    SocialServiceInternal.orderedProviders(onDone);
  },

  getOriginActivationType: function (origin) {
    return getOriginActivationType(origin);
  },

  _providerListeners: new Map(),
  registerProviderListener: function registerProviderListener(listener) {
    this._providerListeners.set(listener, 1);
  },
  unregisterProviderListener: function unregisterProviderListener(listener) {
    this._providerListeners.delete(listener);
  },

  _notifyProviderListeners: function (topic, origin, providers) {
    for (let [listener, ] of this._providerListeners) {
      try {
        listener(topic, origin, providers);
      } catch (ex) {
        Components.utils.reportError("SocialService: provider listener threw an exception: " + ex);
      }
    }
  },

  _manifestFromData: function(type, data, installOrigin) {
    let featureURLs = ['workerURL', 'sidebarURL', 'shareURL', 'statusURL', 'markURL'];
    let resolveURLs = featureURLs.concat(['postActivationURL']);

    if (type == 'directory' || type == 'internal') {
      // directory provided manifests must have origin in manifest, use that
      if (!data['origin']) {
        Cu.reportError("SocialService.manifestFromData directory service provided manifest without origin.");
        return null;
      }
      installOrigin = data.origin;
    }
    // force/fixup origin
    let URI = Services.io.newURI(installOrigin, null, null);
    principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(URI);
    data.origin = principal.origin;

    // iconURL and name are required
    let providerHasFeatures = [url for (url of featureURLs) if (data[url])].length > 0;
    if (!providerHasFeatures) {
      Cu.reportError("SocialService.manifestFromData manifest missing required urls.");
      return null;
    }
    if (!data['name'] || !data['iconURL']) {
      Cu.reportError("SocialService.manifestFromData manifest missing name or iconURL.");
      return null;
    }
    for (let url of resolveURLs) {
      if (data[url]) {
        try {
          let resolved = Services.io.newURI(principal.URI.resolve(data[url]), null, null);
          if (!(resolved.schemeIs("http") || resolved.schemeIs("https"))) {
            Cu.reportError("SocialService.manifestFromData unsupported scheme '" + resolved.scheme + "' for " + principal.origin);
            return null;
          }
          data[url] = resolved.spec;
        } catch(e) {
          Cu.reportError("SocialService.manifestFromData unable to resolve '" + url + "' for " + principal.origin);
          return null;
        }
      }
    }
    return data;
  },

  _showInstallNotification: function(data, aAddonInstaller) {
    let brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    // internal/directory activations need to use the manifest origin, any other
    // use the domain activation is occurring on
    let url = data.url;
    if (data.installType == "internal" || data.installType == "directory") {
      url = data.manifest.origin;
    }
    let requestingURI =  Services.io.newURI(url, null, null);
    let productName = brandBundle.GetStringFromName("brandShortName");

    let message = browserBundle.formatStringFromName("service.install.description",
                                                     [requestingURI.host, productName], 2);

    let action = {
      label: browserBundle.GetStringFromName("service.install.ok.label"),
      accessKey: browserBundle.GetStringFromName("service.install.ok.accesskey"),
      callback: function() {
        aAddonInstaller.install();
      },
    };

    let options = {
                    learnMoreURL: Services.urlFormatter.formatURLPref("app.support.baseURL") + "social-api",
                  };
    let anchor = "servicesInstall-notification-icon";
    let notificationid = "servicesInstall";
    data.window.PopupNotifications.show(data.window.gBrowser.selectedBrowser,
                                        notificationid, message, anchor,
                                        action, [], options);
  },

  installProvider: function(data, installCallback, options={}) {
    data.installType = getOriginActivationType(data.origin);
    // if we get data, we MUST have a valid manifest generated from the data
    let manifest = this._manifestFromData(data.installType, data.manifest, data.origin);
    if (!manifest)
      throw new Error("SocialService.installProvider: service configuration is invalid from " + data.url);

    let addon = new AddonWrapper(manifest);
    if (addon && addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
      throw new Error("installProvider: provider with origin [" +
                      data.origin + "] is blocklisted");
    // manifestFromData call above will enforce correct origin. To support
    // activation from about: uris, we need to be sure to use the updated
    // origin on the manifest.
    data.manifest = manifest;
    let id = getAddonIDFromOrigin(manifest.origin);
    AddonManager.getAddonByID(id, function(aAddon) {
      if (aAddon && aAddon.userDisabled) {
        aAddon.cancelUninstall();
        aAddon.userDisabled = false;
      }
      schedule(function () {
        this._installProvider(data, options, aManifest => {
          this._notifyProviderListeners("provider-installed", aManifest.origin);
          installCallback(aManifest);
        });
      }.bind(this));
    }.bind(this));
  },

  _installProvider: function(data, options, installCallback) {
    if (!data.manifest)
      throw new Error("Cannot install provider without manifest data");

    if (data.installType == "foreign" && !Services.prefs.getBoolPref("social.remote-install.enabled"))
      throw new Error("Remote install of services is disabled");

    let installer = new AddonInstaller(data.url, data.manifest, installCallback);
    let bypassPanel = options.bypassInstallPanel ||
                      (data.installType == "internal" && data.manifest.oneclick);
    if (bypassPanel)
      installer.install();
    else
      this._showInstallNotification(data, installer);
  },

  createWrapper: function(manifest) {
    return new AddonWrapper(manifest);
  },

  /**
   * updateProvider is used from the worker to self-update.  Since we do not
   * have knowledge of the currently selected provider here, we will notify
   * the front end to deal with any reload.
   */
  updateProvider: function(aUpdateOrigin, aManifest) {
    let installType = this.getOriginActivationType(aUpdateOrigin);
    // if we get data, we MUST have a valid manifest generated from the data
    let manifest = this._manifestFromData(installType, aManifest, aUpdateOrigin);
    if (!manifest)
      throw new Error("SocialService.installProvider: service configuration is invalid from " + aUpdateOrigin);

    // overwrite the preference
    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(manifest);
    Services.prefs.setComplexValue(getPrefnameFromOrigin(manifest.origin), Ci.nsISupportsString, string);

    // overwrite the existing provider then notify the front end so it can
    // handle any reload that might be necessary.
    if (ActiveProviders.has(manifest.origin)) {
      // unload the worker prior to replacing the provider instance, also
      // ensures the workerapi instance is terminated.
      let provider = SocialServiceInternal.providers[manifest.origin];
      provider.enabled = false;
      provider = new SocialProvider(manifest);
      SocialServiceInternal.providers[provider.origin] = provider;
      // update the cache and ui, reload provider if necessary
      this.getOrderedProviderList(providers => {
        this._notifyProviderListeners("provider-update", provider.origin, providers);
      });
    }

  },

  uninstallProvider: function(origin, aCallback) {
    let manifest = SocialService.getManifestByOrigin(origin);
    let addon = new AddonWrapper(manifest);
    addon.uninstall(aCallback);
  }
};

/**
 * The SocialProvider object represents a social provider, and allows
 * access to its FrameWorker (if it has one).
 *
 * @constructor
 * @param {jsobj} object representing the manifest file describing this provider
 * @param {bool} boolean indicating whether this provider is "built in"
 */
function SocialProvider(input) {
  if (!input.name)
    throw new Error("SocialProvider must be passed a name");
  if (!input.origin)
    throw new Error("SocialProvider must be passed an origin");

  let addon = new AddonWrapper(input);
  if (addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED)
    throw new Error("SocialProvider: provider with origin [" +
                    input.origin + "] is blocklisted");

  this.name = input.name;
  this.iconURL = input.iconURL;
  this.icon32URL = input.icon32URL;
  this.icon64URL = input.icon64URL;
  this.workerURL = input.workerURL;
  this.sidebarURL = input.sidebarURL;
  this.shareURL = input.shareURL;
  this.statusURL = input.statusURL;
  this.markURL = input.markURL;
  this.markedIcon = input.markedIcon;
  this.unmarkedIcon = input.unmarkedIcon;
  this.postActivationURL = input.postActivationURL;
  this.origin = input.origin;
  let originUri = Services.io.newURI(input.origin, null, null);
  this.principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(originUri);
  this.ambientNotificationIcons = {};
  this.errorState = null;
  this.frecency = 0;

  // this provider has localStorage access in the worker if listed in the
  // whitelist
  let whitelist = Services.prefs.getCharPref("social.whitelist").split(',');
  this.blessed = whitelist.indexOf(this.origin) >= 0;

  try {
    this.domain = etld.getBaseDomainFromHost(originUri.host);
  } catch(e) {
    this.domain = originUri.host;
  }
}

SocialProvider.prototype = {
  reload: function() {
    // calling terminate/activate does not set the enabled state whereas setting
    // enabled will call terminate/activate
    this.enabled = false;
    this.enabled = true;
    Services.obs.notifyObservers(null, "social:provider-reload", this.origin);
  },

  // Provider enabled/disabled state. Disabled providers do not have active
  // connections to their FrameWorkers.
  _enabled: false,
  get enabled() {
    return this._enabled;
  },
  set enabled(val) {
    let enable = !!val;
    if (enable == this._enabled)
      return;

    this._enabled = enable;

    if (enable) {
      this._activate();
    } else {
      this._terminate();
    }
  },

  get manifest() {
    return SocialService.getManifestByOrigin(this.origin);
  },

  getPageSize: function(name) {
    let manifest = this.manifest;
    if (manifest && manifest.pageSize)
      return manifest.pageSize[name];
    return undefined;
  },

  // Reference to a workerAPI object for this provider. Null if the provider has
  // no FrameWorker, or is disabled.
  workerAPI: null,

  // Contains information related to the user's profile. Populated by the
  // workerAPI via updateUserProfile.
  // Properties:
  //   iconURL, portrait, userName, displayName, profileURL
  // See https://github.com/mozilla/socialapi-dev/blob/develop/docs/socialAPI.md
  // A value of null or an empty object means 'user not logged in'.
  // A value of undefined means the service has not yet told us the status of
  // the profile (ie, the service is still loading/initing, or the provider has
  // no FrameWorker)
  // This distinction might be used to cache certain data between runs - eg,
  // browser-social.js caches the notification icons so they can be displayed
  // quickly at startup without waiting for the provider to initialize -
  // 'undefined' means 'ok to use cached values' versus 'null' meaning 'cached
  // values aren't to be used as the user is logged out'.
  profile: undefined,

  // Map of objects describing the provider's notification icons, whose
  // properties include:
  //   name, iconURL, counter, contentPanel
  // See https://developer.mozilla.org/en-US/docs/Social_API
  ambientNotificationIcons: null,

  // Called by the workerAPI to update our profile information.
  updateUserProfile: function(profile) {
    if (!profile)
      profile = {};
    let accountChanged = !this.profile || this.profile.userName != profile.userName;
    this.profile = profile;

    // Sanitize the portrait from any potential script-injection.
    if (profile.portrait) {
      try {
        let portraitUri = Services.io.newURI(profile.portrait, null, null);

        let scheme = portraitUri ? portraitUri.scheme : "";
        if (scheme != "data" && scheme != "http" && scheme != "https") {
          profile.portrait = "";
        }
      } catch (ex) {
        profile.portrait = "";
      }
    }

    if (profile.iconURL)
      this.iconURL = profile.iconURL;

    if (!profile.displayName)
      profile.displayName = profile.userName;

    // if no userName, consider this a logged out state, emtpy the
    // users ambient notifications.  notify both profile and ambient
    // changes to clear everything
    if (!profile.userName) {
      this.profile = {};
      this.ambientNotificationIcons = {};
      Services.obs.notifyObservers(null, "social:ambient-notification-changed", this.origin);
    }

    Services.obs.notifyObservers(null, "social:profile-changed", this.origin);
    if (accountChanged)
      closeAllChatWindows(this);
  },

  haveLoggedInUser: function () {
    return !!(this.profile && this.profile.userName);
  },

  // Called by the workerAPI to add/update a notification icon.
  setAmbientNotification: function(notification) {
    if (!this.profile.userName)
      throw new Error("unable to set notifications while logged out");
    if (!this.ambientNotificationIcons[notification.name] &&
        Object.keys(this.ambientNotificationIcons).length >= 3) {
      throw new Error("ambient notification limit reached");
    }
    this.ambientNotificationIcons[notification.name] = notification;

    Services.obs.notifyObservers(null, "social:ambient-notification-changed", this.origin);
  },

  // Internal helper methods
  _activate: function _activate() {
    // Initialize the workerAPI and its port first, so that its initialization
    // occurs before any other messages are processed by other ports.
    let workerAPIPort = this.getWorkerPort();
    if (workerAPIPort)
      this.workerAPI = new WorkerAPI(this, workerAPIPort);
  },

  _terminate: function _terminate() {
    closeAllChatWindows(this);
    if (this.workerURL) {
      try {
        getFrameWorkerHandle(this.workerURL).terminate();
      } catch (e) {
        Cu.reportError("SocialProvider FrameWorker termination failed: " + e);
      }
    }
    if (this.workerAPI) {
      this.workerAPI.terminate();
    }
    this.errorState = null;
    this.workerAPI = null;
    this.profile = undefined;
  },

  /**
   * Instantiates a FrameWorker for the provider if one doesn't exist, and
   * returns a reference to a new port to that FrameWorker.
   *
   * Returns null if this provider has no workerURL, or is disabled.
   *
   * @param {DOMWindow} window (optional)
   */
  getWorkerPort: function getWorkerPort(window) {
    if (!this.workerURL || !this.enabled)
      return null;
    // Only allow localStorage in the frameworker for blessed providers
    let allowLocalStorage = this.blessed;
    let handle = getFrameWorkerHandle(this.workerURL, window,
                                      "SocialProvider:" + this.origin, this.origin,
                                      allowLocalStorage);
    return handle.port;
  },

  /**
   * Checks if a given URI is of the same origin as the provider.
   *
   * Returns true or false.
   *
   * @param {URI or string} uri
   */
  isSameOrigin: function isSameOrigin(uri, allowIfInheritsPrincipal) {
    if (!uri)
      return false;
    if (typeof uri == "string") {
      try {
        uri = Services.io.newURI(uri, null, null);
      } catch (ex) {
        // an invalid URL can't be loaded!
        return false;
      }
    }
    try {
      this.principal.checkMayLoad(
        uri, // the thing to check.
        false, // reportError - we do our own reporting when necessary.
        allowIfInheritsPrincipal
      );
      return true;
    } catch (ex) {
      return false;
    }
  },

  /**
   * Resolve partial URLs for a provider.
   *
   * Returns nsIURI object or null on failure
   *
   * @param {string} url
   */
  resolveUri: function resolveUri(url) {
    try {
      let fullURL = this.principal.URI.resolve(url);
      return Services.io.newURI(fullURL, null, null);
    } catch (ex) {
      Cu.reportError("mozSocial: failed to resolve window URL: " + url + "; " + ex);
      return null;
    }
  }
}

function getAddonIDFromOrigin(origin) {
  let originUri = Services.io.newURI(origin, null, null);
  return originUri.host + ID_SUFFIX;
}

function getPrefnameFromOrigin(origin) {
  return "social.manifest." + SocialServiceInternal.getManifestPrefname(origin);
}

function AddonInstaller(sourceURI, aManifest, installCallback) {
  aManifest.updateDate = Date.now();
  // get the existing manifest for installDate
  let manifest = SocialService.getManifestByOrigin(aManifest.origin);
  let isNewInstall = !manifest;
  if (manifest && manifest.installDate)
    aManifest.installDate = manifest.installDate;
  else
    aManifest.installDate = aManifest.updateDate;

  this.sourceURI = sourceURI;
  this.install = function() {
    let addon = this.addon;
    if (isNewInstall) {
      AddonManagerPrivate.callInstallListeners("onExternalInstall", null, addon, null, false);
      AddonManagerPrivate.callAddonListeners("onInstalling", addon, false);
    }

    let string = Cc["@mozilla.org/supports-string;1"].
                 createInstance(Ci.nsISupportsString);
    string.data = JSON.stringify(aManifest);
    Services.prefs.setComplexValue(getPrefnameFromOrigin(aManifest.origin), Ci.nsISupportsString, string);

    if (isNewInstall) {
      AddonManagerPrivate.callAddonListeners("onInstalled", addon);
    }
    installCallback(aManifest);
  };
  this.cancel = function() {
    Services.prefs.clearUserPref(getPrefnameFromOrigin(aManifest.origin))
  },
  this.addon = new AddonWrapper(aManifest);
};

var SocialAddonProvider = {
  startup: function() {},

  shutdown: function() {},

  updateAddonAppDisabledStates: function() {
    // we wont bother with "enabling" services that are released from blocklist
    for (let manifest of SocialServiceInternal.manifests) {
      try {
        if (ActiveProviders.has(manifest.origin)) {
          let addon = new AddonWrapper(manifest);
          if (addon.blocklistState != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
            SocialService.disableProvider(manifest.origin);
          }
        }
      } catch(e) {
        Cu.reportError(e);
      }
    }
  },

  getAddonByID: function(aId, aCallback) {
    for (let manifest of SocialServiceInternal.manifests) {
      if (aId == getAddonIDFromOrigin(manifest.origin)) {
        aCallback(new AddonWrapper(manifest));
        return;
      }
    }
    aCallback(null);
  },

  getAddonsByTypes: function(aTypes, aCallback) {
    if (aTypes && aTypes.indexOf(ADDON_TYPE_SERVICE) == -1) {
      aCallback([]);
      return;
    }
    aCallback([new AddonWrapper(a) for each (a in SocialServiceInternal.manifests)]);
  },

  removeAddon: function(aAddon, aCallback) {
    AddonManagerPrivate.callAddonListeners("onUninstalling", aAddon, false);
    aAddon.pendingOperations |= AddonManager.PENDING_UNINSTALL;
    Services.prefs.clearUserPref(getPrefnameFromOrigin(aAddon.manifest.origin));
    aAddon.pendingOperations -= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onUninstalled", aAddon);
    SocialService._notifyProviderListeners("provider-uninstalled", aAddon.manifest.origin);
    if (aCallback)
      schedule(aCallback);
  }
}


function AddonWrapper(aManifest) {
  this.manifest = aManifest;
  this.id = getAddonIDFromOrigin(this.manifest.origin);
  this._pending = AddonManager.PENDING_NONE;
}
AddonWrapper.prototype = {
  get type() {
    return ADDON_TYPE_SERVICE;
  },

  get appDisabled() {
    return this.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  set softDisabled(val) {
    this.userDisabled = val;
  },

  get softDisabled() {
    return this.userDisabled;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get scope() {
    return AddonManager.SCOPE_PROFILE;
  },

  get foreignInstall() {
    return false;
  },

  isCompatibleWith: function(appVersion, platformVersion) {
    return true;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get blocklistState() {
    return Services.blocklist.getAddonBlocklistState(this);
  },

  get blocklistURL() {
    return Services.blocklist.getAddonBlocklistURL(this);
  },

  get screenshots() {
    return [];
  },

  get pendingOperations() {
    return this._pending || AddonManager.PENDING_NONE;
  },
  set pendingOperations(val) {
    this._pending = val;
  },

  get operationsRequiringRestart() {
    return AddonManager.OP_NEEDS_RESTART_NONE;
  },

  get size() {
    return null;
  },

  get permissions() {
    let permissions = 0;
    // any "user defined" manifest can be removed
    if (Services.prefs.prefHasUserValue(getPrefnameFromOrigin(this.manifest.origin)))
      permissions = AddonManager.PERM_CAN_UNINSTALL;
    if (!this.appDisabled) {
      if (this.userDisabled) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      } else {
        permissions |= AddonManager.PERM_CAN_DISABLE;
      }
    }
    return permissions;
  },

  findUpdates: function(listener, reason, appVersion, platformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in listener)
      listener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in listener)
      listener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in listener)
      listener.onUpdateFinished(this);
  },

  get isActive() {
    return ActiveProviders.has(this.manifest.origin);
  },

  get name() {
    return this.manifest.name;
  },
  get version() {
    return this.manifest.version ? this.manifest.version : "";
  },

  get iconURL() {
    return this.manifest.icon32URL ? this.manifest.icon32URL : this.manifest.iconURL;
  },
  get icon64URL() {
    return this.manifest.icon64URL;
  },
  get icons() {
    let icons = {
      16: this.manifest.iconURL
    };
    if (this.manifest.icon32URL)
      icons[32] = this.manifest.icon32URL;
    if (this.manifest.icon64URL)
      icons[64] = this.manifest.icon64URL;
    return icons;
  },

  get description() {
    return this.manifest.description;
  },
  get homepageURL() {
    return this.manifest.homepageURL;
  },
  get defaultLocale() {
    return this.manifest.defaultLocale;
  },
  get selectedLocale() {
    return this.manifest.selectedLocale;
  },

  get installDate() {
    return this.manifest.installDate ? new Date(this.manifest.installDate) : null;
  },
  get updateDate() {
    return this.manifest.updateDate ? new Date(this.manifest.updateDate) : null;
  },

  get creator() {
    return new AddonManagerPrivate.AddonAuthor(this.manifest.author);
  },

  get userDisabled() {
    return this.appDisabled || !ActiveProviders.has(this.manifest.origin);
  },

  set userDisabled(val) {
    if (val == this.userDisabled)
      return val;
    if (val) {
      SocialService.disableProvider(this.manifest.origin);
    } else if (!this.appDisabled) {
      SocialService.enableProvider(this.manifest.origin);
    }
    return val;
  },

  uninstall: function(aCallback) {
    let prefName = getPrefnameFromOrigin(this.manifest.origin);
    if (Services.prefs.prefHasUserValue(prefName)) {
      if (ActiveProviders.has(this.manifest.origin)) {
        SocialService.disableProvider(this.manifest.origin, function() {
          SocialAddonProvider.removeAddon(this, aCallback);
        }.bind(this));
      } else {
        SocialAddonProvider.removeAddon(this, aCallback);
      }
    } else {
      schedule(aCallback);
    }
  },

  cancelUninstall: function() {
    this._pending -= AddonManager.PENDING_UNINSTALL;
    AddonManagerPrivate.callAddonListeners("onOperationCancelled", this);
  }
};


AddonManagerPrivate.registerProvider(SocialAddonProvider, [
  new AddonManagerPrivate.AddonType(ADDON_TYPE_SERVICE, URI_EXTENSION_STRINGS,
                                    STRING_TYPE_NAME,
                                    AddonManager.VIEW_TYPE_LIST, 10000)
]);
