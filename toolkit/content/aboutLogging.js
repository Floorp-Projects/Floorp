/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);
const gDirServ = Cc["@mozilla.org/file/directory_service;1"].getService(
  Ci.nsIDirectoryServiceProvider
);

const $ = document.querySelector.bind(document);

const gLoggingPresets = {
  networking: {
    modules:
      "timestamp,sync,nsHttp:5,cache2:5,nsSocketTransport:5,nsHostResolver:5",
    l10nIds: {
      label: "about-logging-preset-networking-label",
      description: "about-logging-preset-networking-description",
    },
  },
  "media-playback": {
    modules:
      "cubeb:5,PlatformDecoderModule:5,AudioSink:5,AudioSinkWrapper:5,MediaDecoderStateMachine:4,MediaDecoder:4",
    l10nIds: {
      label: "about-logging-preset-media-playback-label",
      description: "about-logging-preset-media-playback-description",
    },
  },
  custom: {
    modules: "",
    l10nIds: {
      label: "about-logging-preset-custom-label",
      description: "about-logging-preset-custom-description",
    },
  },
};

function populatePresets() {
  let dropdown = $("#logging-preset-dropdown");
  for (let presetName in gLoggingPresets) {
    let preset = gLoggingPresets[presetName];
    let option = document.createElement("option");
    document.l10n.setAttributes(option, preset.l10nIds.label);
    option.value = presetName;
    dropdown.appendChild(option);
  }

  function setPresetAndDescription(preset) {
    document.l10n.setAttributes(
      $("#logging-preset-description"),
      gLoggingPresets[preset].l10nIds.description
    );
  }

  dropdown.onchange = function() {
    $("#log-modules").value = gLoggingPresets[dropdown.value].modules;
    setPresetAndDescription(dropdown.value);
    Services.prefs.setCharPref("logging.config.preset", dropdown.value);
    setLogModules();
  };

  $("#log-modules").value = gLoggingPresets[dropdown.value].modules;
  setPresetAndDescription(dropdown.value);
  $("#log-modules").oninput = e => {
    dropdown.value = "custom";
  };
}

let gInited = false;
function init() {
  if (gInited) {
    return;
  }
  gInited = true;
  gDashboard.enableLogging = true;

  let setLogButton = document.getElementById("set-log-file-button");
  setLogButton.addEventListener("click", setLogFile);

  let setModulesButton = document.getElementById("set-log-modules-button");
  setModulesButton.addEventListener("click", setLogModules);

  let startLoggingButton = document.getElementById("start-logging-button");
  startLoggingButton.addEventListener("click", startLogging);

  let stopLoggingButton = document.getElementById("stop-logging-button");
  stopLoggingButton.addEventListener("click", stopLogging);

  try {
    let file = gDirServ.getFile("TmpD", {});
    file.append("log.txt");
    document.getElementById("log-file").value = file.path;
  } catch (e) {
    console.error(e);
  }

  populatePresets();

  // Update the value of the log file.
  updateLogFile();

  // Update the active log modules
  updateLogModules();

  // If we can't set the file and the modules at runtime,
  // the start and stop buttons wouldn't really do anything.
  if (setLogButton.disabled || setModulesButton.disabled) {
    document.querySelector("#buttons-disabled").hidden = false;
    startLoggingButton.disabled = true;
    stopLoggingButton.disabled = true;
  }
}

function updateLogFile() {
  let logPath = "";

  // Try to get the environment variable for the log file
  logPath =
    Services.env.get("MOZ_LOG_FILE") || Services.env.get("NSPR_LOG_FILE");
  let currentLogFile = document.getElementById("current-log-file");
  let setLogFileButton = document.getElementById("set-log-file-button");

  // If the log file was set from an env var, we disable the ability to set it
  // at runtime.
  if (logPath.length) {
    currentLogFile.innerText = logPath;
    setLogFileButton.disabled = true;
  } else if (gDashboard.getLogPath() != ".moz_log") {
    // There may be a value set by a pref.
    currentLogFile.innerText = gDashboard.getLogPath();
  } else {
    try {
      let file = gDirServ.getFile("TmpD", {});
      file.append("log.txt");
      document.getElementById("log-file").value = file.path;
    } catch (e) {
      console.error(e);
    }
    // Fall back to the temp dir
    currentLogFile.innerText = document.getElementById("log-file").value;
  }

  let openLogFileButton = document.getElementById("open-log-file-button");
  openLogFileButton.disabled = true;

  if (currentLogFile.innerText.length) {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(currentLogFile.innerText);

    if (file.exists()) {
      openLogFileButton.disabled = false;
      openLogFileButton.onclick = function(e) {
        file.reveal();
      };
    }
  }
}

function updateLogModules() {
  // Try to get the environment variable for the log file
  let logModules =
    Services.env.get("MOZ_LOG") ||
    Services.env.get("MOZ_LOG_MODULES") ||
    Services.env.get("NSPR_LOG_MODULES");
  let currentLogModules = document.getElementById("current-log-modules");
  let setLogModulesButton = document.getElementById("set-log-modules-button");
  if (logModules.length) {
    currentLogModules.innerText = logModules;
    // If the log modules are set by an environment variable at startup, do not
    // allow changing them throught a pref. It would be difficult to figure out
    // which ones are enabled and which ones are not. The user probably knows
    // what he they are doing.
    setLogModulesButton.disabled = true;
  } else {
    let activeLogModules = [];
    try {
      if (Services.prefs.getBoolPref("logging.config.add_timestamp")) {
        activeLogModules.push("timestamp");
      }
    } catch (e) {}
    try {
      if (Services.prefs.getBoolPref("logging.config.sync")) {
        activeLogModules.push("sync");
      }
    } catch (e) {}
    try {
      if (Services.prefs.getBoolPref("logging.config.profilerstacks")) {
        activeLogModules.push("profilerstacks");
      }
    } catch (e) {}

    let children = Services.prefs.getBranch("logging.").getChildList("");

    for (let pref of children) {
      if (pref.startsWith("config.")) {
        continue;
      }

      try {
        let value = Services.prefs.getIntPref(`logging.${pref}`);
        activeLogModules.push(`${pref}:${value}`);
      } catch (e) {
        console.error(e);
      }
    }

    if (activeLogModules.length !== 0) {
      currentLogModules.innerText = activeLogModules.join(",");
      currentLogModules.hidden = false;
      $("#no-log-modules").hidden = true;
    } else {
      currentLogModules.innerText = "";
      currentLogModules.hidden = true;
      $("#no-log-modules").hidden = false;
    }
  }
}

function setLogFile() {
  let setLogButton = document.getElementById("set-log-file-button");
  if (setLogButton.disabled) {
    // There's no point trying since it wouldn't work anyway.
    return;
  }
  let logFile = document.getElementById("log-file").value.trim();
  Services.prefs.setCharPref("logging.config.LOG_FILE", logFile);
  updateLogFile();
}

function clearLogModules() {
  // Turn off all the modules.
  let children = Services.prefs.getBranch("logging.").getChildList("");
  for (let pref of children) {
    if (!pref.startsWith("config.")) {
      Services.prefs.clearUserPref(`logging.${pref}`);
    }
  }
  Services.prefs.clearUserPref("logging.config.add_timestamp");
  Services.prefs.clearUserPref("logging.config.sync");
  updateLogModules();
}

function setLogModules() {
  let setLogModulesButton = document.getElementById("set-log-modules-button");
  if (setLogModulesButton.disabled) {
    // The modules were set via env var, so we shouldn't try to change them.
    return;
  }

  let modules = document.getElementById("log-modules").value.trim();

  // Clear previously set log modules.
  clearLogModules();

  if (modules.length !== 0) {
    let logModules = modules.split(",");
    for (let module of logModules) {
      if (module == "timestamp") {
        Services.prefs.setBoolPref("logging.config.add_timestamp", true);
      } else if (module == "rotate") {
        // XXX: rotate is not yet supported.
      } else if (module == "append") {
        // XXX: append is not yet supported.
      } else if (module == "sync") {
        Services.prefs.setBoolPref("logging.config.sync", true);
      } else if (module == "profilerstacks") {
        Services.prefs.setBoolPref("logging.config.profilerstacks", true);
      } else {
        let lastColon = module.lastIndexOf(":");
        let key = module.slice(0, lastColon);
        let value = parseInt(module.slice(lastColon + 1), 10);
        Services.prefs.setIntPref(`logging.${key}`, value);
      }
    }
  }

  updateLogModules();
}

function startLogging() {
  setLogFile();
  setLogModules();
}

function stopLogging() {
  clearLogModules();
  // clear the log file as well
  Services.prefs.clearUserPref("logging.config.LOG_FILE");
  updateLogFile();
}

// We use the pageshow event instead of onload. This is needed because sometimes
// the page is loaded via session-restore/bfcache. In such cases we need to call
// init() to keep the page behaviour consistent with the ticked checkboxes.
// Mostly the issue is with the autorefresh checkbox.
window.addEventListener("pageshow", function() {
  init();
});
