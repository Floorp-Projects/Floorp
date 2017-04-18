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
  Debug.uninit();
}

function onLoad() {
  window.removeEventListener("load", onLoad);
  window.addEventListener("unload", unLoad);

  Provider.init();
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
    elem.childNodes[0].nodeValue = aData;
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

          let str = bundle.GetStringFromName("TriggerUpdate")
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
    let table = Services.prefs.getCharPref(pref, "").split(",")[0];

    let updateUrl = listmanager.getUpdateUrl(table);
    if (!listmanager.checkForUpdates(updateUrl)) {
      // This may because of back-off algorithm.
      let elem = document.getElementById(provider + "-col-lastupdateresult");
      elem.childNodes[0].nodeValue = bundle.GetStringFromName("CannotUpdate");
    }
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
      chk.addEventListener("click", () => { logModuleUpdate(module) });
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
