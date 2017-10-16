/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const bundle = Services.strings.createBundle(
  "chrome://global/locale/aboutUrlClassifier.properties");

const UPDATE_BEGIN = "safebrowsing-update-begin";
const UPDATE_FINISH = "safebrowsing-update-finished";
const JSLOG_PREF = "browser.safebrowsing.debug";

const STR_NA = bundle.GetStringFromName("NotAvailable");

function unLoad() {
  window.removeEventListener("unload", unLoad);

  Provider.uninit();
  Cache.uninit();
  Debug.uninit();
}

function onLoad() {
  window.removeEventListener("load", onLoad);
  window.addEventListener("unload", unLoad);

  Provider.init();
  Cache.init();
  Debug.init();
}

/*
 * Provider
 */
var Provider = {
  providers: null,

  updatingProvider: "",

  init() {
    this.providers = new Set();
    let branch = Services.prefs.getBranch("browser.safebrowsing.provider.");
    let children = branch.getChildList("", {});
    for (let child of children) {
      this.providers.add(child.split(".")[0]);
    }

    this.register();
    this.render();
    this.refresh();
  },

  uninit() {
    Services.obs.removeObserver(this.onBeginUpdate, UPDATE_BEGIN);
    Services.obs.removeObserver(this.onFinishUpdate, UPDATE_FINISH);
  },

  onBeginUpdate(aSubject, aTopic, aData) {
    this.updatingProvider = aData;
    let p = this.updatingProvider;

    // Disable update button for the provider while we are doing update.
    document.getElementById("update-" + p).disabled = true;

    let elem = document.getElementById(p + "-col-lastupdateresult");
    elem.childNodes[0].nodeValue = bundle.GetStringFromName("Updating");
  },

  onFinishUpdate(aSubject, aTopic, aData) {
    let p = this.updatingProvider;
    this.updatingProvider = "";

    // It is possible that we get update-finished event only because
    // about::url-classifier is opened after update-begin event is fired.
    if (p === "") {
      this.refresh();
      return;
    }

    this.refresh([p]);

    document.getElementById("update-" + p).disabled = false;

    let elem = document.getElementById(p + "-col-lastupdateresult");
    if (aData.startsWith("success")) {
      elem.childNodes[0].nodeValue = bundle.GetStringFromName("success");
    } else if (aData.startsWith("update error")) {
      elem.childNodes[0].nodeValue =
        bundle.formatStringFromName("updateError", [aData.split(": ")[1]], 1);
    } else if (aData.startsWith("download error")) {
      elem.childNodes[0].nodeValue =
        bundle.formatStringFromName("downloadError", [aData.split(": ")[1]], 1);
    } else {
      elem.childNodes[0].nodeValue = aData;
    }
  },

  register() {
    // Handle begin update
    this.onBeginUpdate = this.onBeginUpdate.bind(this);
    Services.obs.addObserver(this.onBeginUpdate, UPDATE_BEGIN);

    // Handle finish update
    this.onFinishUpdate = this.onFinishUpdate.bind(this);
    Services.obs.addObserver(this.onFinishUpdate, UPDATE_FINISH);
  },

  // This should only be called once because we assume number of providers
  // won't change.
  render() {
    let tbody = document.getElementById("provider-table-body");

    for (let provider of this.providers) {
      let tr = document.createElement("tr");
      let cols = document.getElementById("provider-head-row").childNodes;
      for (let column of cols) {
        if (!column.id) {
          continue;
        }
        let td = document.createElement("td");
        td.id = provider + "-" + column.id;

        if (column.id === "col-update") {
          let btn = document.createElement("button");
          btn.id = "update-" + provider;
          btn.addEventListener("click", () => { this.update(provider); });

          let str = bundle.GetStringFromName("TriggerUpdate");
          btn.appendChild(document.createTextNode(str));
          td.appendChild(btn);
        } else {
          let str = column.id === "col-lastupdateresult" ? STR_NA : "";
          td.appendChild(document.createTextNode(str));
        }
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }
  },

  refresh(listProviders = this.providers) {
    for (let provider of listProviders) {
      let values = {};
      values["col-provider"] = provider;

      let pref = "browser.safebrowsing.provider." + provider + ".lastupdatetime";
      let lut = Services.prefs.getCharPref(pref, "");
      values["col-lastupdatetime"] = lut ? new Date(lut * 1) : STR_NA;

      pref = "browser.safebrowsing.provider." + provider + ".nextupdatetime";
      let nut = Services.prefs.getCharPref(pref, "");
      values["col-nextupdatetime"] = nut ? new Date(nut * 1) : STR_NA;

      let listmanager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                        .getService(Ci.nsIUrlListManager);
      let bot = listmanager.getBackOffTime(provider);
      values["col-backofftime"] = bot ? new Date(bot * 1) : STR_NA;

      for (let key of Object.keys(values)) {
        let elem = document.getElementById(provider + "-" + key);
        elem.childNodes[0].nodeValue = values[key];
      }
    }
  },

  // Call update for the provider.
  update(provider) {
    let listmanager = Cc["@mozilla.org/url-classifier/listmanager;1"]
                      .getService(Ci.nsIUrlListManager);

    let pref = "browser.safebrowsing.provider." + provider + ".lists";
    let tables = Services.prefs.getCharPref(pref, "").split(",");
    let table = tables.find(t => listmanager.getUpdateUrl(t) != "");

    let updateUrl = listmanager.getUpdateUrl(table);
    if (!listmanager.checkForUpdates(updateUrl)) {
      // This may because of back-off algorithm.
      let elem = document.getElementById(provider + "-col-lastupdateresult");
      elem.childNodes[0].nodeValue = bundle.GetStringFromName("CannotUpdate");
    }
  },

};

/*
 * Cache
 */
var Cache = {
  // Tables that show cahe entries.
  showCacheEnties: null,

  init() {
    this.showCacheEnties = new Set();

    this.register();
    this.render();
  },

  uninit() {
    Services.obs.removeObserver(this.refresh, UPDATE_FINISH);
  },

  register() {
    this.refresh = this.refresh.bind(this);
    Services.obs.addObserver(this.refresh, UPDATE_FINISH);
  },

  render() {
    this.createCacheEntries();

    let refreshBtn = document.getElementById("refresh-cache-btn");
    refreshBtn.addEventListener("click", () => { this.refresh(); });

    let clearBtn = document.getElementById("clear-cache-btn");
    clearBtn.addEventListener("click", () => {
      let dbservice = Cc["@mozilla.org/url-classifier/dbservice;1"]
                      .getService(Ci.nsIUrlClassifierDBService);
      dbservice.clearCache();
      // Since clearCache is async call, we just simply assume it will be
      // updated in 100 milli-seconds.
      setTimeout(() => { this.refresh(); }, 100);
    });
  },

  refresh() {
    this.clearCacheEntries();
    this.createCacheEntries();
  },

  clearCacheEntries() {
    let ctbody = document.getElementById("cache-table-body");
    while (ctbody.firstChild) {
      ctbody.firstChild.remove();
    }

    let cetbody = document.getElementById("cache-entries-table-body");
    while (cetbody.firstChild) {
      cetbody.firstChild.remove();
    }
  },

  createCacheEntries() {
    function createRow(tds, body, cols) {
      let tr = document.createElement("tr");
      tds.forEach(function(v, i, a) {
        let td = document.createElement("td");
        if (i == 0 && tds.length != cols) {
          td.setAttribute("colspan", cols - tds.length + 1);
        }
        let elem = typeof v === "object" ? v : document.createTextNode(v);
        td.appendChild(elem);
        tr.appendChild(td);
      });
      body.appendChild(tr);
    }

    let dbservice = Cc["@mozilla.org/url-classifier/dbservice;1"]
                    .getService(Ci.nsIUrlClassifierInfo);

    for (let provider of Provider.providers) {
      let pref = "browser.safebrowsing.provider." + provider + ".lists";
      let tables = Services.prefs.getCharPref(pref, "").split(",");

      for (let table of tables) {
        let cache = dbservice.getCacheInfo(table);
        let entries = cache.entries;
        if (entries.length === 0) {
          this.showCacheEnties.delete(table);
          continue;
        }

        let positiveCacheCount = 0;
        for (let i = 0; i < entries.length ; i++) {
          let entry = entries.queryElementAt(i, Ci.nsIUrlClassifierCacheEntry);
          let matches = entry.matches;
          positiveCacheCount += matches.length;

          // If we don't have to show cache entries for this table then just
          // skip the following code.
          if (!this.showCacheEnties.has(table)) {
            continue;
          }

          let tds = [table, entry.prefix, new Date(entry.expiry * 1000).toString()];
          let j = 0;
          do {
            if (matches.length >= 1) {
              let match =
                matches.queryElementAt(j, Ci.nsIUrlClassifierPositiveCacheEntry);
              let list = [match.fullhash, new Date(match.expiry * 1000).toString()];
              tds = tds.concat(list);
            } else {
              tds = tds.concat([STR_NA, STR_NA]);
            }
            createRow(tds, document.getElementById("cache-entries-table-body"), 5);
            j++;
            tds = [""];
          } while (j < matches.length);
        }

        // Create cache information entries.
        let chk = document.createElement("input");
        chk.type = "checkbox";
        chk.checked = this.showCacheEnties.has(table);
        chk.addEventListener("click", () => {
          if (chk.checked) {
            this.showCacheEnties.add(table);
          } else {
            this.showCacheEnties.delete(table);
          }
          this.refresh();
        });

        let tds = [table, entries.length, positiveCacheCount, chk];
        createRow(tds, document.getElementById("cache-table-body"), tds.length);
      }
    }

    let entries_div = document.getElementById("cache-entries");
    entries_div.style.display = this.showCacheEnties.size == 0 ? "none" : "block";
  },
};

/*
 * Debug
 */
var Debug = {
  // url-classifier NSPR Log modules.
  modules: ["UrlClassifierDbService",
            "nsChannelClassifier",
            "UrlClassifierProtocolParser",
            "UrlClassifierStreamUpdater",
            "UrlClassifierPrefixSet",
            "ApplicationReputation"],

  init() {
    this.register();
    this.render();
    this.refresh();
  },

  uninit() {
    Services.prefs.removeObserver(JSLOG_PREF, this.refreshJSDebug);
  },

  register() {
    this.refreshJSDebug = this.refreshJSDebug.bind(this);
    Services.prefs.addObserver(JSLOG_PREF, this.refreshJSDebug);
  },

  render() {
    // This function update the log module text field if we click
    // safebrowsing log module check box.
    function logModuleUpdate(module) {
      let txt = document.getElementById("log-modules");
      let chk = document.getElementById("chk-" + module);

      let dst = chk.checked ? "," + module + ":5" : "";
      let re = new RegExp(",?" + module + ":[0-9]");

      let str = txt.value.replace(re, dst);
      if (chk.checked) {
        str = txt.value === str ? str + dst : str;
      }
      txt.value = str.replace(/^,/, "");
    }

    let setLog = document.getElementById("set-log-modules");
    setLog.addEventListener("click", this.nsprlog);

    let setLogFile = document.getElementById("set-log-file");
    setLogFile.addEventListener("click", this.logfile);

    let setJSLog = document.getElementById("js-log");
    setJSLog.addEventListener("click", this.jslog);

    let modules = document.getElementById("log-modules");
    let sbModules = document.getElementById("sb-log-modules");
    for (let module of this.modules) {
      let container = document.createElement("div");
      container.className = "toggle-container-with-text";
      sbModules.appendChild(container);

      let chk = document.createElement("input");
      chk.id = "chk-" + module;
      chk.type = "checkbox";
      chk.checked = true;
      chk.addEventListener("click", () => { logModuleUpdate(module); });
      container.appendChild(chk, modules);

      let label = document.createElement("label");
      label.for = chk.id;
      label.appendChild(document.createTextNode(module));
      container.appendChild(label, modules);
    }

    this.modules.map(logModuleUpdate);

    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append("safebrowsing.log");

    let logFile = document.getElementById("log-file");
    logFile.value = file.path;

    let curLog = document.getElementById("cur-log-modules");
    curLog.childNodes[0].nodeValue = "";

    let curLogFile = document.getElementById("cur-log-file");
    curLogFile.childNodes[0].nodeValue = "";
  },

  refresh() {
    this.refreshJSDebug();

    // Disable configure log modules if log modules are already set
    // by environment variable.
    let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);

    let logModules = env.get("MOZ_LOG") ||
                     env.get("MOZ_LOG_MODULES") ||
                     env.get("NSPR_LOG_MODULES");

    if (logModules.length > 0) {
      document.getElementById("set-log-modules").disabled = true;
      for (let module of this.modules) {
        document.getElementById("chk-" + module).disabled = true;
      }

      let curLogModules = document.getElementById("cur-log-modules");
      curLogModules.childNodes[0].nodeValue = logModules;
    }

    // Disable set log file if log file is already set
    // by environment variable.
    let logFile = env.get("MOZ_LOG_FILE") || env.get("NSPR_LOG_FILE");
    if (logFile.length > 0) {
      document.getElementById("set-log-file").disabled = true;
      document.getElementById("log-file").value = logFile;
    }
  },

  refreshJSDebug() {
    let enabled = Services.prefs.getBoolPref(JSLOG_PREF, false);

    let jsChk = document.getElementById("js-log");
    jsChk.checked = enabled;

    let curJSLog = document.getElementById("cur-js-log");
    curJSLog.childNodes[0].nodeValue = enabled ?
      bundle.GetStringFromName("Enabled") :
      bundle.GetStringFromName("Disabled");
  },

  jslog() {
    let enabled = Services.prefs.getBoolPref(JSLOG_PREF, false);
    Services.prefs.setBoolPref(JSLOG_PREF, !enabled);
  },

  nsprlog() {
    // Turn off debugging for all the modules.
    let children = Services.prefs.getBranch("logging.").getChildList("", {});
    for (let pref of children) {
      if (!pref.startsWith("config.")) {
        Services.prefs.clearUserPref(`logging.${pref}`);
      }
    }

    let value = document.getElementById("log-modules").value;
    let logModules = value.split(",");
    for (let module of logModules) {
      let [key, value] = module.split(":");
      Services.prefs.setIntPref(`logging.${key}`, parseInt(value, 10));
    }

    let curLogModules = document.getElementById("cur-log-modules");
    curLogModules.childNodes[0].nodeValue = value;
  },

  logfile() {
    let logFile = document.getElementById("log-file").value.trim();
    Services.prefs.setCharPref("logging.config.LOG_FILE", logFile);

    let curLogFile = document.getElementById("cur-log-file");
    curLogFile.childNodes[0].nodeValue = logFile;
  }
};
