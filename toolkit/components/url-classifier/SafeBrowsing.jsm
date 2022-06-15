/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SafeBrowsing"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_DEBUG_ENABLED = "browser.safebrowsing.debug";
let loggingEnabled = false;

// Log only if browser.safebrowsing.debug is true
function log(...stuff) {
  if (!loggingEnabled) {
    return;
  }

  var d = new Date();
  let msg = "SafeBrowsing: " + d.toTimeString() + ": " + stuff.join(" ");
  dump(Services.urlFormatter.trimSensitiveURLs(msg) + "\n");
}

function getLists(prefName) {
  log("getLists: " + prefName);
  let pref = Services.prefs.getCharPref(prefName, "");

  // Splitting an empty string returns [''], we really want an empty array.
  if (!pref) {
    return [];
  }

  return pref.split(",").map(value => value.trim());
}

const FEATURES = [
  {
    name: "phishing",
    list: ["urlclassifier.phishTable"],
    enabled() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.phishing.enabled"
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.phishing.update",
        this.enabled()
      );
    },
  },
  {
    name: "malware",
    list: ["urlclassifier.malwareTable"],
    enabled() {
      return Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled");
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.malware.update",
        this.enabled()
      );
    },
  },
  {
    name: "blockedURIs",
    list: ["urlclassifier.blockedTable"],
    enabled() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.blockedURIs.enabled"
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.blockedURIs.update",
        this.enabled()
      );
    },
  },
  {
    name: "passwords",
    list: ["urlclassifier.passwordAllowTable"],
    enabled() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.passwords.enabled"
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.passwords.update",
        this.enabled()
      );
    },
  },
  {
    name: "downloads",
    list: [
      "urlclassifier.downloadBlockTable",
      "urlclassifier.downloadAllowTable",
    ],
    enabled() {
      return (
        Services.prefs.getBoolPref("browser.safebrowsing.downloads.enabled") &&
        Services.prefs.getBoolPref("browser.safebrowsing.malware.enabled")
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.downloads.update",
        this.enabled()
      );
    },
  },
  {
    name: "trackingAnnotation",
    list: [
      "urlclassifier.trackingAnnotationTable",
      "urlclassifier.trackingAnnotationWhitelistTable",
    ],
    enabled() {
      return Services.prefs.getBoolPref(
        "privacy.trackingprotection.annotate_channels"
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.trackingAnnotation.update",
        this.enabled()
      );
    },
  },
  {
    name: "trackingProtection",
    list: [
      "urlclassifier.trackingTable",
      "urlclassifier.trackingWhitelistTable",
    ],
    enabled() {
      return (
        Services.prefs.getBoolPref("privacy.trackingprotection.enabled") ||
        Services.prefs.getBoolPref("privacy.trackingprotection.pbmode.enabled")
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.trackingProtection.update",
        this.enabled()
      );
    },
  },
  {
    name: "fingerprinting-annotation",
    list: [
      "urlclassifier.features.fingerprinting.annotate.blacklistTables",
      "urlclassifier.features.fingerprinting.annotate.whitelistTables",
    ],
    enabled() {
      // Annotation features are enabled by default.
      return true;
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.fingerprinting.annotate.update",
        this.enabled()
      );
    },
  },
  {
    name: "fingerprinting-protection",
    list: [
      "urlclassifier.features.fingerprinting.blacklistTables",
      "urlclassifier.features.fingerprinting.whitelistTables",
    ],
    enabled() {
      return Services.prefs.getBoolPref(
        "privacy.trackingprotection.fingerprinting.enabled",
        false
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.fingerprinting.update",
        this.enabled()
      );
    },
  },
  {
    name: "cryptomining-annotation",
    list: [
      "urlclassifier.features.cryptomining.annotate.blacklistTables",
      "urlclassifier.features.cryptomining.annotate.whitelistTables",
    ],
    enabled() {
      // Annotation features are enabled by default.
      return true;
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.cryptomining.annotate.update",
        this.enabled()
      );
    },
  },
  {
    name: "cryptomining-protection",
    list: [
      "urlclassifier.features.cryptomining.blacklistTables",
      "urlclassifier.features.cryptomining.whitelistTables",
    ],
    enabled() {
      return Services.prefs.getBoolPref(
        "privacy.trackingprotection.cryptomining.enabled",
        false
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.cryptomining.update",
        this.enabled()
      );
    },
  },
  {
    name: "socialtracking-annotation",
    list: [
      "urlclassifier.features.socialtracking.annotate.blacklistTables",
      "urlclassifier.features.socialtracking.annotate.whitelistTables",
    ],
    enabled() {
      // Annotation features are enabled by default.
      return true;
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.socialtracking.annotate.update",
        this.enabled()
      );
    },
  },
  {
    name: "socialtracking-protection",
    list: [
      "urlclassifier.features.socialtracking.blacklistTables",
      "urlclassifier.features.socialtracking.whitelistTables",
    ],
    enabled() {
      return Services.prefs.getBoolPref(
        "privacy.trackingprotection.socialtracking.enabled",
        false
      );
    },
    update() {
      return Services.prefs.getBoolPref(
        "browser.safebrowsing.features.socialtracking.update",
        this.enabled()
      );
    },
  },
];

var SafeBrowsing = {
  init() {
    if (this.initialized) {
      log("Already initialized");
      return;
    }

    Services.prefs.addObserver("browser.safebrowsing", this);
    Services.prefs.addObserver("privacy.trackingprotection", this);
    Services.prefs.addObserver("urlclassifier", this);

    this.readPrefs();

    this.controlUpdateChecking();
    this.initialized = true;

    log("init() finished");
  },

  registerTableWithURLs(listname) {
    let listManager = Cc[
      "@mozilla.org/url-classifier/listmanager;1"
    ].getService(Ci.nsIUrlListManager);

    let providerName = this.listToProvider[listname];
    let provider = this.providers[providerName];

    if (!providerName || !provider) {
      log("No provider info found for " + listname);
      log("Check browser.safebrowsing.provider.[google/mozilla].lists");
      return;
    }

    if (!provider.updateURL) {
      log("Invalid update url " + listname);
      return;
    }

    listManager.registerTable(
      listname,
      providerName,
      provider.updateURL,
      provider.gethashURL
    );
  },

  registerTables() {
    this.features.forEach(feature => {
      feature.list.forEach(table => {
        this.registerTableWithURLs(table);
      });
    });
  },

  unregisterTables(obsoleteLists) {
    let listManager = Cc[
      "@mozilla.org/url-classifier/listmanager;1"
    ].getService(Ci.nsIUrlListManager);

    obsoleteLists.forEach(list => {
      list.forEach(table => {
        listManager.unregisterTable(table);
      });
    });
  },

  initialized: false,

  features: [],

  updateURL: null,
  gethashURL: null,
  reportURL: null,

  getReportURL(kind, info) {
    let pref;
    switch (kind) {
      case "Phish":
        pref = "browser.safebrowsing.reportPhishURL";
        break;

      case "PhishMistake":
      case "MalwareMistake":
        pref =
          "browser.safebrowsing.provider." +
          info.provider +
          ".report" +
          kind +
          "URL";
        break;

      default:
        let err =
          "SafeBrowsing getReportURL() called with unknown kind: " + kind;
        Cu.reportError(err);
        throw err;
    }

    // The "Phish" reports are about submitting new phishing URLs to Google so
    // they don't have an associated list URL
    if (kind != "Phish" && (!info.list || !info.uri)) {
      return null;
    }

    let reportUrl = Services.urlFormatter.formatURLPref(pref);
    // formatURLPref might return "about:blank" if getting the pref fails
    if (reportUrl == "about:blank") {
      reportUrl = null;
    }

    if (reportUrl) {
      reportUrl += encodeURIComponent(info.uri);
    }
    return reportUrl;
  },

  observe(aSubject, aTopic, aData) {
    // skip nextupdatetime and lastupdatetime
    if (aData.includes("lastupdatetime") || aData.includes("nextupdatetime")) {
      return;
    }

    if (aData == PREF_DEBUG_ENABLED) {
      loggingEnabled = Services.prefs.getBoolPref(PREF_DEBUG_ENABLED);
      return;
    }

    this.readPrefs();
  },

  readPrefs() {
    loggingEnabled = Services.prefs.getBoolPref(PREF_DEBUG_ENABLED);
    log("reading prefs");

    let obsoleteLists = [];
    // Make a copy of the original lists before we re-read the prefs.
    if (this.initialized) {
      obsoleteLists = this.features.map(feature => {
        return feature.list;
      });
    }

    // Allow to disable all feature updates with a single preference for tests.
    let update = Services.prefs.getBoolPref(
      "browser.safebrowsing.update.enabled",
      true
    );

    this.features = [];
    for (let i = 0; i < FEATURES.length; ++i) {
      this.features[i] = {
        name: FEATURES[i].name,
        list: [],
        enabled: FEATURES[i].enabled(),
        update: FEATURES[i].update() && update,
      };

      FEATURES[i].list.forEach(pref => {
        this.features[i].list.push(...getLists(pref));
      });
    }

    for (let i = 0; i < obsoleteLists.length; ++i) {
      obsoleteLists[i] = obsoleteLists[i].filter(
        list => !this.features[i].list.includes(list)
      );
    }

    this.updateProviderURLs();
    this.registerTables();
    if (obsoleteLists) {
      this.unregisterTables(obsoleteLists);
    }

    // XXX The listManager backend gets confused if this is called before the
    // lists are registered. So only call it here when a pref changes, and not
    // when doing initialization. I expect to refactor this later, so pardon the hack.
    if (this.initialized) {
      this.controlUpdateChecking();
    }
  },

  updateProviderURLs() {
    try {
      var clientID = Services.prefs.getCharPref("browser.safebrowsing.id");
    } catch (e) {
      clientID = Services.appinfo.name;
    }

    log("initializing safe browsing URLs, client id", clientID);

    // Get the different providers
    let branch = Services.prefs.getBranch("browser.safebrowsing.provider.");
    let children = branch.getChildList("");
    this.providers = {};
    this.listToProvider = {};

    for (let child of children) {
      log("Child: " + child);
      let prefComponents = child.split(".");
      let providerName = prefComponents[0];
      this.providers[providerName] = {};
    }

    if (loggingEnabled) {
      let providerStr = "";
      Object.keys(this.providers).forEach(function(provider) {
        if (providerStr === "") {
          providerStr = provider;
        } else {
          providerStr += ", " + provider;
        }
      });
      log("Providers: " + providerStr);
    }

    Object.keys(this.providers).forEach(function(provider) {
      if (provider == "test") {
        return; // skip
      }
      let updateURL = Services.urlFormatter.formatURLPref(
        "browser.safebrowsing.provider." + provider + ".updateURL"
      );
      let gethashURL = Services.urlFormatter.formatURLPref(
        "browser.safebrowsing.provider." + provider + ".gethashURL"
      );
      updateURL = updateURL.replace("SAFEBROWSING_ID", clientID);
      gethashURL = gethashURL.replace("SAFEBROWSING_ID", clientID);

      // Disable updates and gethash if the Google API key is missing.
      let googleSafebrowsingKey = Services.urlFormatter
        .formatURL("%GOOGLE_SAFEBROWSING_API_KEY%")
        .trim();
      if (
        (provider == "google" || provider == "google4") &&
        (!googleSafebrowsingKey ||
          googleSafebrowsingKey == "no-google-safebrowsing-api-key")
      ) {
        log(
          "Missing Google SafeBrowsing API key, clearing updateURL and gethashURL."
        );
        updateURL = "";
        gethashURL = "";
      }

      log("Provider: " + provider + " updateURL=" + updateURL);
      log("Provider: " + provider + " gethashURL=" + gethashURL);

      // Urls used to update DB
      this.providers[provider].updateURL = updateURL;
      this.providers[provider].gethashURL = gethashURL;

      // Get lists this provider manages
      let lists = getLists(
        "browser.safebrowsing.provider." + provider + ".lists"
      );
      if (lists) {
        lists.forEach(function(list) {
          this.listToProvider[list] = provider;
        }, this);
      } else {
        log("Update URL given but no lists managed for provider: " + provider);
      }
    }, this);
  },

  controlUpdateChecking() {
    if (loggingEnabled) {
      this.features.forEach(feature => {
        log("feature " + feature.name + ":");
        log("  enabled:" + feature.enabled);
        log("  update:" + feature.update);
        log("  tables:" + feature.list);
      });
    }

    let listManager = Cc[
      "@mozilla.org/url-classifier/listmanager;1"
    ].getService(Ci.nsIUrlListManager);

    listManager.disableAllUpdates();

    this.features.forEach(feature => {
      if (feature.update) {
        feature.list.forEach(table => {
          listManager.enableUpdate(table);
        });
      }
    });

    listManager.maybeToggleUpdateChecking();
  },
};
