/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

function updatePrefsAndDefines() {
  let upload = Services.prefs.getBoolPref(
    "datareporting.healthreport.uploadEnabled"
  );
  document.l10n.setAttributes(
    document.querySelector("[data-l10n-id='about-glean-data-upload']"),
    "about-glean-data-upload",
    {
      "data-upload-pref-value": upload,
    }
  );
  let port = Services.prefs.getIntPref("telemetry.fog.test.localhost_port");
  document.l10n.setAttributes(
    document.querySelector("[data-l10n-id='about-glean-local-port']"),
    "about-glean-local-port",
    {
      "local-port-pref-value": port,
    }
  );
  document.l10n.setAttributes(
    document.querySelector("[data-l10n-id='about-glean-glean-android']"),
    "about-glean-glean-android",
    { "glean-android-define-value": AppConstants.MOZ_GLEAN_ANDROID }
  );
  document.l10n.setAttributes(
    document.querySelector("[data-l10n-id='about-glean-moz-official']"),
    "about-glean-moz-official",
    { "moz-official-define-value": AppConstants.MOZILLA_OFFICIAL }
  );

  // Knowing what we know, and copying logic from viaduct_uploader.rs,
  // (which is documented in Preferences and Defines),
  // tell the fine user whether and why upload is disabled.
  let uploadMessageEl = document.getElementById("upload-status");
  let uploadL10nId = "about-glean-upload-enabled";
  if (!upload) {
    uploadL10nId = "about-glean-upload-disabled";
  } else if (port < 0 || (port == 0 && !AppConstants.MOZILLA_OFFICIAL)) {
    uploadL10nId = "about-glean-upload-fake-enabled";
    // This message has a link to the Glean Debug Ping Viewer in it.
    // We must add the anchor element now so that Fluent can match it.
    let a = document.createElement("a");
    a.href = "https://debug-ping-preview.firebaseapp.com/";
    a.setAttribute("data-l10n-name", "glean-debug-ping-viewer");
    uploadMessageEl.appendChild(a);
  } else if (port > 0) {
    uploadL10nId = "about-glean-upload-enabled-local";
  }
  document.l10n.setAttributes(uploadMessageEl, uploadL10nId);
}

function camelToKebab(str) {
  let out = "";
  for (let i = 0; i < str.length; i++) {
    let c = str.charAt(i);
    if (c == c.toUpperCase()) {
      out += "-";
      c = c.toLowerCase();
    }
    out += c;
  }
  return out;
}

// I'm consciously omitting "deletion-request" until someone can come up with
// a use-case for sending it via about:glean.
const GLEAN_BUILTIN_PINGS = ["metrics", "events", "baseline"];
const NO_PING = "(don't submit any ping)";
function refillPingNames() {
  let select = document.getElementById("ping-names");
  let pings = GLEAN_BUILTIN_PINGS.slice().concat(Object.keys(GleanPings));

  pings.forEach(ping => {
    let option = document.createElement("option");
    option.textContent = camelToKebab(ping);
    select.appendChild(option);
  });
  let option = document.createElement("option");
  document.l10n.setAttributes(option, "about-glean-no-ping-label");
  option.value = NO_PING;
  select.appendChild(option);
}

// If there's been a previous tag, use it.
// If not, be _slightly_ clever and derive a default one from the profile dir.
function fillDebugTag() {
  const DEBUG_TAG_PREF = "telemetry.fog.aboutGlean.debugTag";
  let debugTag;
  if (Services.prefs.prefHasUserValue(DEBUG_TAG_PREF)) {
    debugTag = Services.prefs.getStringPref(DEBUG_TAG_PREF);
  } else {
    const debugTagPrefix = "about-glean-";
    const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    let charSum = Array.from(profileDir).reduce(
      (prev, cur) => prev + cur.charCodeAt(0),
      0
    );

    debugTag = debugTagPrefix + (charSum % 1000);
  }

  let tagInput = document.getElementById("tag-pings");
  tagInput.value = debugTag;
  const updateDebugTagValues = () => {
    document.l10n.setAttributes(
      document.querySelector(
        "[data-l10n-id='about-glean-label-for-controls-submit']"
      ),
      "about-glean-label-for-controls-submit",
      { "debug-tag": tagInput.value }
    );
    const GDPV_ROOT = "https://debug-ping-preview.firebaseapp.com/pings/";
    let gdpvLink = document.querySelector(
      "[data-l10n-name='gdpv-tagged-pings-link']"
    );
    gdpvLink.href = GDPV_ROOT + tagInput.value;
  };
  tagInput.addEventListener("change", () => {
    Services.prefs.setStringPref(DEBUG_TAG_PREF, tagInput.value);
    updateDebugTagValues();
  });
  updateDebugTagValues();
}

function onLoad() {
  updatePrefsAndDefines();
  refillPingNames();
  fillDebugTag();
  document.getElementById("controls-submit").addEventListener("click", () => {
    let tag = document.getElementById("tag-pings").value;
    let log = document.getElementById("log-pings").checked;
    let ping = document.getElementById("ping-names").value;
    Services.fog.setLogPings(log);
    Services.fog.setTagPings(tag);
    if (ping != NO_PING) {
      Services.fog.sendPing(ping);
    }
  });
}

window.addEventListener("load", onLoad);
