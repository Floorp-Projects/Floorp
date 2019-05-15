/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  WindowsGPOParser: "resource://gre/modules/policies/WindowsGPOParser.jsm",
  macOSPoliciesParser: "resource://gre/modules/policies/macOSPoliciesParser.jsm",
  Policies: "resource:///modules/policies/Policies.jsm",
  JsonSchemaValidator: "resource://gre/modules/components-utils/JsonSchemaValidator.jsm",
});

// This is the file that will be searched for in the
// ${InstallDir}/distribution folder.
const POLICIES_FILENAME = "policies.json";

// For easy testing, modify the helpers/sample.json file,
// and set PREF_ALTERNATE_PATH in firefox.js as:
// /your/repo/browser/components/enterprisepolicies/helpers/sample.json
const PREF_ALTERNATE_PATH     = "browser.policies.alternatePath";
// For testing, we may want to set PREF_ALTERNATE_PATH to point to a file
// relative to the test root directory. In order to enable this, the string
// below may be placed at the beginning of that preference value and it will
// be replaced with the path to the test root directory.
const MAGIC_TEST_ROOT_PREFIX  = "<test-root>";
const PREF_TEST_ROOT          = "mochitest.testRoot";

const PREF_LOGLEVEL           = "browser.policies.loglevel";

// To force disallowing enterprise-only policies during tests
const PREF_DISALLOW_ENTERPRISE = "browser.policies.testing.disallowEnterprise";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "Enterprise Policies",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
const isXpcshell = env.exists("XPCSHELL_TEST_PROFILE_DIR");

function EnterprisePoliciesManager() {
  Services.obs.addObserver(this, "profile-after-change", true);
  Services.obs.addObserver(this, "final-ui-startup", true);
  Services.obs.addObserver(this, "sessionstore-windows-restored", true);
  Services.obs.addObserver(this, "EnterprisePolicies:Restart", true);
}

EnterprisePoliciesManager.prototype = {
  classID:        Components.ID("{ea4e1414-779b-458b-9d1f-d18e8efbc145}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIEnterprisePolicies]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(EnterprisePoliciesManager),

  _initialize() {
    let provider = this._chooseProvider();

    if (!provider) {
      this.status = Ci.nsIEnterprisePolicies.INACTIVE;
      return;
    }

    if (provider.failed) {
      this.status = Ci.nsIEnterprisePolicies.FAILED;
      return;
    }

    this.status = Ci.nsIEnterprisePolicies.ACTIVE;
    this._parsedPolicies = {};
    this._activatePolicies(provider.policies);
  },

  _chooseProvider() {
    let provider = null;
    if (AppConstants.platform == "win") {
      provider = new WindowsGPOPoliciesProvider();
    } else if (AppConstants.platform == "macosx") {
      provider = new macOSPoliciesProvider();
    }
    if (provider && provider.hasPolicies) {
      return provider;
    }

    provider = new JSONPoliciesProvider();
    if (provider.hasPolicies) {
      return provider;
    }

    return null;
  },

  _activatePolicies(unparsedPolicies) {
    let { schema } = ChromeUtils.import("resource:///modules/policies/schema.jsm");

    for (let policyName of Object.keys(unparsedPolicies)) {
      let policySchema = schema.properties[policyName];
      let policyParameters = unparsedPolicies[policyName];

      if (!policySchema) {
        log.error(`Unknown policy: ${policyName}`);
        continue;
      }

      if (policySchema.enterprise_only && !areEnterpriseOnlyPoliciesAllowed()) {
        log.error(`Policy ${policyName} is only allowed on ESR`);
        continue;
      }

      let [parametersAreValid, parsedParameters] =
        JsonSchemaValidator.validateAndParseParameters(policyParameters, policySchema);

      if (!parametersAreValid) {
        log.error(`Invalid parameters specified for ${policyName}.`);
        continue;
      }

      this._parsedPolicies[policyName] = parsedParameters;
      let policyImpl = Policies[policyName];

      for (let timing of Object.keys(this._callbacks)) {
        let policyCallback = policyImpl[timing];
        if (policyCallback) {
          this._schedulePolicyCallback(
            timing,
            policyCallback.bind(policyImpl,
                                this, /* the EnterprisePoliciesManager */
                                parsedParameters));
        }
      }
    }
  },

  _callbacks: {
    // The earliest that a policy callback can run. This will
    // happen right after the Policy Engine itself has started,
    // and before the Add-ons Manager has started.
    onBeforeAddons: [],

    // This happens after all the initialization related to
    // the profile has finished (prefs, places database, etc.).
    onProfileAfterChange: [],

    // Just before the first browser window gets created.
    onBeforeUIStartup: [],

    // Called after all windows from the last session have been
    // restored (or the default window and homepage tab, if the
    // session is not being restored).
    // The content of the tabs themselves have not necessarily
    // finished loading.
    onAllWindowsRestored: [],
  },

  _schedulePolicyCallback(timing, callback) {
    this._callbacks[timing].push(callback);
  },

  _runPoliciesCallbacks(timing) {
    let callbacks = this._callbacks[timing];
    while (callbacks.length > 0) {
      let callback = callbacks.shift();
      try {
        callback();
      } catch (ex) {
        log.error("Error running ", callback, `for ${timing}:`, ex);
      }
    }
  },

  async _restart() {
    DisallowedFeatures = {};

    Services.ppmm.sharedData.delete("EnterprisePolicies:Status");
    Services.ppmm.sharedData.delete("EnterprisePolicies:DisallowedFeatures");

    this._status = Ci.nsIEnterprisePolicies.UNINITIALIZED;
    for (let timing of Object.keys(this._callbacks)) {
      this._callbacks[timing] = [];
    }

    let { PromiseUtils } = ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
    // Simulate the startup process. This step-by-step is a bit ugly but it
    // tries to emulate the same behavior as of a normal startup.

    await PromiseUtils.idleDispatch(() => {
      this.observe(null, "policies-startup", null);
    });

    await PromiseUtils.idleDispatch(() => {
      this.observe(null, "profile-after-change", null);
    });

    await PromiseUtils.idleDispatch(() => {
      this.observe(null, "final-ui-startup", null);
    });

    await PromiseUtils.idleDispatch(() => {
      this.observe(null, "sessionstore-windows-restored", null);
    });
  },

  // nsIObserver implementation
  observe: function BG_observe(subject, topic, data) {
    switch (topic) {
      case "policies-startup":
        // Before the first set of policy callbacks runs, we must
        // initialize the service.
        this._initialize();

        this._runPoliciesCallbacks("onBeforeAddons");
        break;

      case "profile-after-change":
        this._runPoliciesCallbacks("onProfileAfterChange");
        break;

      case "final-ui-startup":
        this._runPoliciesCallbacks("onBeforeUIStartup");
        break;

      case "sessionstore-windows-restored":
        this._runPoliciesCallbacks("onAllWindowsRestored");

        // After the last set of policy callbacks ran, notify the test observer.
        Services.obs.notifyObservers(null,
                                     "EnterprisePolicies:AllPoliciesApplied");
        break;

      case "EnterprisePolicies:Restart":
        this._restart().then(null, Cu.reportError);
        break;
    }
  },

  disallowFeature(feature, neededOnContentProcess = false) {
    DisallowedFeatures[feature] = neededOnContentProcess;

    // NOTE: For optimization purposes, only features marked as needed
    // on content process will be passed onto the child processes.
    if (neededOnContentProcess) {
      Services.ppmm.sharedData.set("EnterprisePolicies:DisallowedFeatures",
        new Set(Object.keys(DisallowedFeatures).filter(key => DisallowedFeatures[key])));
    }
  },

  // ------------------------------
  // public nsIEnterprisePolicies members
  // ------------------------------

  _status: Ci.nsIEnterprisePolicies.UNINITIALIZED,

  set status(val) {
    this._status = val;
    if (val != Ci.nsIEnterprisePolicies.INACTIVE) {
      Services.ppmm.sharedData.set("EnterprisePolicies:Status", val);
    }
    return val;
  },

  get status() {
    return this._status;
  },

  isAllowed: function BG_sanitize(feature) {
    return !(feature in DisallowedFeatures);
  },

  getActivePolicies() {
    return this._parsedPolicies;
  },

  setSupportMenu(supportMenu) {
    SupportMenu = supportMenu;
  },

  getSupportMenu() {
    return SupportMenu;
  },

  setExtensionPolicies(extensionPolicies) {
    ExtensionPolicies = extensionPolicies;
  },

  getExtensionPolicy(extensionID) {
    if (ExtensionPolicies &&
        extensionID in ExtensionPolicies) {
      return ExtensionPolicies[extensionID];
    }
    return null;
  },

  setExtensionSettings(extensionSettings) {
    ExtensionSettings = extensionSettings;
    if ("*" in extensionSettings &&
        "install_sources" in extensionSettings["*"]) {
      InstallSources = new MatchPatternSet(extensionSettings["*"].install_sources);
    }
  },

  getExtensionSettings(extensionID) {
    let settings = null;
    if (ExtensionSettings) {
      if (extensionID in ExtensionSettings) {
        settings = ExtensionSettings[extensionID];
      } else if ("*" in ExtensionSettings) {
        settings = ExtensionSettings["*"];
      }
    }
    return settings;
  },

  mayInstallAddon(addon) {
    // See https://dev.chromium.org/administrators/policy-list-3/extension-settings-full
    if (!ExtensionSettings) {
      return true;
    }
    if (addon.id in ExtensionSettings) {
      if ("installation_mode" in ExtensionSettings[addon.id]) {
        switch (ExtensionSettings[addon.id].installation_mode) {
          case "blocked":
            return false;
          default:
            return true;
        }
      }
    }
    if ("*" in ExtensionSettings) {
      if (ExtensionSettings["*"].installation_mode &&
          ExtensionSettings["*"].installation_mode == "blocked") {
        return false;
      }
      if ("allowed_types" in ExtensionSettings["*"]) {
        return ExtensionSettings["*"].allowed_types.includes(addon.type);
      }
    }
    return true;
  },

  allowedInstallSource(uri) {
    return InstallSources ? InstallSources.matches(uri) : true;
  },
};

let DisallowedFeatures = {};
let SupportMenu = null;
let ExtensionPolicies = null;
let ExtensionSettings = null;
let InstallSources = null;

/**
 * areEnterpriseOnlyPoliciesAllowed
 *
 * Checks whether the policies marked as enterprise_only in the
 * schema are allowed to run on this browser.
 *
 * This is meant to only allow policies to run on ESR, but in practice
 * we allow it to run on channels different than release, to allow
 * these policies to be tested on pre-release channels.
 *
 * @returns {Bool} Whether the policy can run.
 */
function areEnterpriseOnlyPoliciesAllowed() {
  if (Services.prefs.getBoolPref(PREF_DISALLOW_ENTERPRISE, false)) {
    // This is used as an override to test the "enterprise_only"
    // functionality itself on tests, which would always return
    // true due to the Cu.isInAutomation check below.
    return false;
  }

  if (AppConstants.MOZ_UPDATE_CHANNEL != "release" ||
      Cu.isInAutomation || isXpcshell) {
    return true;
  }

  return false;
}

/*
 * JSON PROVIDER OF POLICIES
 *
 * This is a platform-agnostic provider which looks for
 * policies specified through a policies.json file stored
 * in the installation's distribution folder.
 */

class JSONPoliciesProvider {
  constructor() {
    this._policies = null;
    this._failed = false;
    this._readData();
  }

  get hasPolicies() {
    return this._policies !== null || this._failed;
  }

  get policies() {
    return this._policies;
  }

  get failed() {
    return this._failed;
  }

  _getConfigurationFile() {
    let configFile = null;
    try {
      configFile = Services.dirsvc.get("XREAppDist", Ci.nsIFile);
      configFile.append(POLICIES_FILENAME);
    } catch (ex) {
      // Getting the correct directory will fail in xpcshell tests. This should
      // be handled the same way as if the configFile simply does not exist.
    }

    let alternatePath = Services.prefs.getStringPref(PREF_ALTERNATE_PATH, "");

    // Check if we are in automation *before* we use the synchronous
    // nsIFile.exists() function or allow the config file to be overriden
    // An alternate policy path can also be used in Nightly builds (for
    // testing purposes), but the Background Update Agent will be unable to
    // detect the alternate policy file so the DisableAppUpdate policy may not
    // work as expected.
    if (alternatePath && (Cu.isInAutomation || AppConstants.NIGHTLY_BUILD || isXpcshell) &&
        (!configFile || !configFile.exists())) {
      if (alternatePath.startsWith(MAGIC_TEST_ROOT_PREFIX)) {
        // Intentionally not using a default value on this pref lookup. If no
        // test root is set, we are not currently testing and this function
        // should throw rather than returning something.
        let testRoot = Services.prefs.getStringPref(PREF_TEST_ROOT);
        let relativePath = alternatePath.substring(MAGIC_TEST_ROOT_PREFIX.length);
        if (AppConstants.platform == "win") {
          relativePath = relativePath.replace(/\//g, "\\");
        }
        alternatePath = testRoot + relativePath;
      }

      configFile = Cc["@mozilla.org/file/local;1"]
                     .createInstance(Ci.nsIFile);
      configFile.initWithPath(alternatePath);
    }

    return configFile;
  }

  _readData() {
    let configFile = this._getConfigurationFile();
    if (!configFile) {
      // Do nothing, _policies will remain null
      return;
    }
    try {
      let data = Cu.readUTF8File(configFile);
      if (data) {
        this._policies = JSON.parse(data).policies;

        if (!this._policies) {
          log.error("Policies file doesn't contain a 'policies' object");
          this._failed = true;
        }
      }
    } catch (ex) {
      if (ex instanceof Components.Exception &&
          ex.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        // Do nothing, _policies will remain null
      } else if (ex instanceof SyntaxError) {
        log.error("Error parsing JSON file");
        this._failed = true;
      } else {
        log.error("Error reading file");
        this._failed = true;
      }
    }
  }
}

class WindowsGPOPoliciesProvider {
  constructor() {
    this._policies = null;

    let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(Ci.nsIWindowsRegKey);

    // Machine policies override user policies, so we read
    // user policies first and then replace them if necessary.
    log.debug("root = HKEY_CURRENT_USER");
    this._readData(wrk, wrk.ROOT_KEY_CURRENT_USER);
    log.debug("root = HKEY_LOCAL_MACHINE");
    this._readData(wrk, wrk.ROOT_KEY_LOCAL_MACHINE);
  }

  get hasPolicies() {
    return this._policies !== null;
  }

  get policies() {
    return this._policies;
  }

  get failed() {
    return this._failed;
  }

  _readData(wrk, root) {
    wrk.open(root, "SOFTWARE\\Policies", wrk.ACCESS_READ);
    if (wrk.hasChild("Mozilla\\" + Services.appinfo.name)) {
      this._policies = WindowsGPOParser.readPolicies(wrk, this._policies);
    }
    wrk.close();
  }
}

class macOSPoliciesProvider {
  constructor() {
    this._policies = null;
    let prefReader = Cc["@mozilla.org/mac-preferences-reader;1"]
                       .createInstance(Ci.nsIMacPreferencesReader);
    if (!prefReader.policiesEnabled()) {
      return;
    }
    this._policies = macOSPoliciesParser.readPolicies(prefReader);
  }

  get hasPolicies() {
    return this._policies !== null;
  }

  get policies() {
    return this._policies;
  }

  get failed() {
    return this._failed;
  }
}

var components = [EnterprisePoliciesManager];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
