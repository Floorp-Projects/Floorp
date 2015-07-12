/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils, Cr = Components.results;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function init() {
  // Include the build date and a warning about Telemetry
  // if this is an "a#" (nightly or aurora) build
#expand const version = "__MOZ_APP_VERSION_DISPLAY__";
  if (/a\d+$/.test(version)) {
    let buildID = Services.appinfo.appBuildID;
    let buildDate = buildID.slice(0, 4) + "-" + buildID.slice(4, 6) + "-" + buildID.slice(6, 8);
    let br = document.createElement("br");
    let versionPara = document.getElementById("version");
    versionPara.appendChild(br);
    let date = document.createTextNode("(" + buildDate + ")");
    versionPara.appendChild(date);
    document.getElementById("telemetry").hidden = false;
  }

  // Include the Distribution information if available
  try {
    let distroId = Services.prefs.getCharPref("distribution.id");
    if (distroId) {
      let distroVersion = Services.prefs.getCharPref("distribution.version");
      let distroIdField = document.getElementById("distributionID");
      distroIdField.textContent = distroId + " - " + distroVersion;
      distroIdField.hidden = false;

      let distroAbout = Services.prefs.getComplexValue("distribution.about", Ci.nsISupportsString);
      let distroField = document.getElementById("distributionAbout");
      distroField.textContent = distroAbout;
      distroField.hidden = false;
    }
  } catch (e) {
    // Pref is unset
  }

  // get URLs from prefs
  try {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);

    let links = [
      {id: "releaseNotesURL", pref: "app.releaseNotesURL"},
      {id: "supportURL",      pref: "app.support.baseURL"},
      {id: "faqURL",          pref: "app.faqURL"},
      {id: "privacyURL",      pref: "app.privacyURL"},
      {id: "creditsURL",      pref: "app.creditsURL"},
    ];

    links.forEach(function(link) {
      let url = formatter.formatURLPref(link.pref);
      let element = document.getElementById(link.id);
      element.setAttribute("href", url);
    });
  } catch (ex) {}

#ifdef MOZ_UPDATER
  let Updater = {
    update: null,

    init: function() {
      Services.obs.addObserver(this, "Update:CheckResult", false);
    },

    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "Update:CheckResult") {
        showUpdateMessage(aData);
      }
    },
  };

  Updater.init();

  function checkForUpdates() {
    showCheckingMessage();

    Services.androidBridge.handleGeckoMessage({ type: "Update:Check" });
  }

  function downloadUpdate() {
    Services.androidBridge.handleGeckoMessage({ type: "Update:Download" });
  }

  function installUpdate() {
    showCheckAction();

    Services.androidBridge.handleGeckoMessage({ type: "Update:Install" });
  }

  let updateLink = document.getElementById("updateLink");
  let checkingSpan = document.getElementById("update-message-checking");
  let noneSpan = document.getElementById("update-message-none");
  let foundSpan = document.getElementById("update-message-found");
  let downloadingSpan = document.getElementById("update-message-downloading");
  let downloadedSpan = document.getElementById("update-message-downloaded");

  updateLink.onclick = checkForUpdates;
  foundSpan.onclick = downloadUpdate;
  downloadedSpan.onclick = installUpdate;

  function showCheckAction() {
    checkingSpan.style.display = "none";
    noneSpan.style.display = "none";
    foundSpan.style.display = "none";
    downloadingSpan.style.display = "none";
    downloadedSpan.style.display = "none";
    updateLink.style.display = "block";
  }

  function showCheckingMessage() {
    updateLink.style.display = "none";
    noneSpan.style.display = "none";
    foundSpan.style.display = "none";
    downloadingSpan.style.display = "none";
    downloadedSpan.style.display = "none";
    checkingSpan.style.display = "block";
  }

  function showUpdateMessage(aResult) {
    updateLink.style.display = "none";
    checkingSpan.style.display = "none";
    noneSpan.style.display = "none";
    foundSpan.style.display = "none";
    downloadingSpan.style.display = "none";
    downloadedSpan.style.display = "none";

    // the aResult values come from mobile/android/base/UpdateServiceHelper.java
    switch (aResult) {
      case "NOT_AVAILABLE":
        noneSpan.style.display = "block";
        setTimeout(showCheckAction, 2000);
        break;
      case "AVAILABLE":
        foundSpan.style.display = "block";
        break;
      case "DOWNLOADING":
        downloadingSpan.style.display = "block";
        break;
      case "DOWNLOADED":
        downloadedSpan.style.display = "block";
        break;
    }
  }
#endif
}

document.addEventListener("DOMContentLoaded", init, false);
