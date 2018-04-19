/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(
  this, "AppConstants", "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this, "AddonStudies", "resource://normandy/lib/AddonStudies.jsm"
);
ChromeUtils.defineModuleGetter(
  this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm"
);

var EXPORTED_SYMBOLS = ["ShieldPreferences"];

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed"; // from modules/libpref/nsIPrefBranch.idl
const FHR_UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const OPT_OUT_STUDIES_ENABLED_PREF = "app.shield.optoutstudies.enabled";
const NORMANDY_ENABLED_PREF = "app.normandy.enabled";

/**
 * Handles Shield-specific preferences, including their UI.
 */
var ShieldPreferences = {
  init() {
    // Watch for changes to the FHR pref
    Services.prefs.addObserver(FHR_UPLOAD_ENABLED_PREF, this);
    CleanupManager.addCleanupHandler(() => {
      Services.prefs.removeObserver(FHR_UPLOAD_ENABLED_PREF, this);
    });

    // Watch for changes to the Opt-out pref
    Services.prefs.addObserver(OPT_OUT_STUDIES_ENABLED_PREF, this);
    CleanupManager.addCleanupHandler(() => {
      Services.prefs.removeObserver(OPT_OUT_STUDIES_ENABLED_PREF, this);
    });

    // Disabled outside of en-* locales temporarily (bug 1377192).
    // Disabled when MOZ_DATA_REPORTING is false since the FHR UI is also hidden
    // when data reporting is false.
    if (AppConstants.MOZ_DATA_REPORTING && Services.locale.getAppLocaleAsLangTag().startsWith("en")) {
      Services.obs.addObserver(this, "privacy-pane-loaded");
      CleanupManager.addCleanupHandler(() => {
        Services.obs.removeObserver(this, "privacy-pane-loaded");
      });
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      // Add the opt-out-study checkbox to the Privacy preferences when it is shown.
      case "privacy-pane-loaded":
        this.injectOptOutStudyCheckbox(subject.document);
        break;
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        this.observePrefChange(data);
        break;
    }
  },

  async observePrefChange(prefName) {
    let prefValue;
    switch (prefName) {
      // If the opt-out pref changes to be false, disable all current studies.
      case OPT_OUT_STUDIES_ENABLED_PREF: {
        prefValue = Services.prefs.getBoolPref(OPT_OUT_STUDIES_ENABLED_PREF);
        if (!prefValue) {
          for (const study of await AddonStudies.getAll()) {
            if (study.active) {
              await AddonStudies.stop(study.recipeId, "general-opt-out");
            }
          }
        }
        break;
      }
    }
  },

  /**
   * Injects the opt-out-study preference checkbox into about:preferences and
   * handles events coming from the UI for it.
   */
  injectOptOutStudyCheckbox(doc) {
    const allowedByPolicy = Services.policies.isAllowed("Shield");

    const container = doc.createElementNS(XUL_NS, "vbox");
    container.classList.add("indent");

    const hContainer = doc.createElementNS(XUL_NS, "hbox");
    hContainer.setAttribute("align", "center");
    container.appendChild(hContainer);

    const checkbox = doc.createElementNS(XUL_NS, "checkbox");
    checkbox.setAttribute("id", "optOutStudiesEnabled");
    checkbox.setAttribute("class", "tail-with-learn-more");
    checkbox.setAttribute("label", "Allow Firefox to install and run studies");
    hContainer.appendChild(checkbox);

    const viewStudies = doc.createElementNS(XUL_NS, "label");
    viewStudies.setAttribute("id", "viewShieldStudies");
    viewStudies.setAttribute("href", "about:studies");
    viewStudies.setAttribute("useoriginprincipal", true);
    viewStudies.textContent = "View Firefox Studies";
    viewStudies.classList.add("learnMore", "text-link");
    hContainer.appendChild(viewStudies);

    // Preference instances for prefs that we need to monitor while the page is open.
    doc.defaultView.Preferences.add({ id: OPT_OUT_STUDIES_ENABLED_PREF, type: "bool" });

    // Weirdly, FHR doesn't have a Preference instance on the page, so we create it.
    const fhrPref = doc.defaultView.Preferences.add({ id: FHR_UPLOAD_ENABLED_PREF, type: "bool" });
    function updateStudyCheckboxState() {
      // The checkbox should be disabled if any of the below are true. This
      // prevents the user from changing the value in the box.
      //
      // * the policy forbids shield
      // * the Shield Study preference is locked
      // * the FHR pref is false
      //
      // The checkbox should match the value of the preference only if all of
      // these are true. Otherwise, the checkbox should remain unchecked. This
      // is because in these situations, Shield studies are always disabled, and
      // so showing a checkbox would be confusing.
      //
      // * MOZ_TELEMETRY_REPORTING is true
      // * the policy allows Shield
      // * the FHR pref is true
      // * Normandy is enabled

      const checkboxMatchesPref = (
        AppConstants.MOZ_DATA_REPORTING &&
        allowedByPolicy &&
        Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF, false) &&
        Services.prefs.getBoolPref(NORMANDY_ENABLED_PREF, false)
      );

      if (checkboxMatchesPref) {
        if (Services.prefs.getBoolPref(OPT_OUT_STUDIES_ENABLED_PREF)) {
          checkbox.setAttribute("checked", "checked");
        } else {
          checkbox.removeAttribute("checked");
        }
        checkbox.setAttribute("preference", OPT_OUT_STUDIES_ENABLED_PREF);
      } else {
        checkbox.removeAttribute("preference");
        checkbox.removeAttribute("checked");
      }

      const isDisabled = (
        !allowedByPolicy ||
        Services.prefs.prefIsLocked(OPT_OUT_STUDIES_ENABLED_PREF) ||
        !Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED_PREF)
      );

      // We can't use checkbox.disabled here because the XBL binding may not be present,
      // in which case setting the property won't work properly.
      if (isDisabled) {
        checkbox.setAttribute("disabled", "true");
      } else {
        checkbox.removeAttribute("disabled");
      }
    }
    fhrPref.on("change", updateStudyCheckboxState);
    updateStudyCheckboxState();
    doc.defaultView.addEventListener("unload", () => fhrPref.off("change", updateStudyCheckboxState), { once: true });

    // Actually inject the elements we've created.
    const parent = doc.getElementById("submitHealthReportBox").closest("description");
    parent.appendChild(container);
  },
};
