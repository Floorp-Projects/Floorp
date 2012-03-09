/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

let Cc = Components.classes;
let Ci = Components.interfaces;

// based on onImportItemsPageShow from migration.js
function populateResetPane(aContainerID) {
  let resetProfileItems = document.getElementById(aContainerID);
  try {
    let dataTypes = getMigratedData();
    for (let dataType of dataTypes) {
      let label = document.createElement("label");
      label.setAttribute("value", dataType);
      resetProfileItems.appendChild(label);
    }
  } catch (ex) {
    Cu.reportError(ex);
  }
}

function onResetProfileLoad() {
  populateResetPane("migratedItems");
}

/**
 * Check if reset is supported for the currently running profile.
 *
 * @return boolean whether reset is supported.
 */
function resetSupported() {
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].
                       getService(Ci.nsIToolkitProfileService);
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

#expand const MOZ_APP_NAME = "__MOZ_APP_NAME__";
#expand const MOZ_BUILD_APP = "__MOZ_BUILD_APP__";

  // Reset is only supported for the default profile if the self-migrator used for reset exists.
  try {
    return currentProfileDir.equals(profileService.selectedProfile.rootDir) &&
             ("@mozilla.org/profile/migrator;1?app=" + MOZ_BUILD_APP + "&type=" + MOZ_APP_NAME in Cc);
  } catch (e) {
    // Catch exception when there is no selected profile.
    Cu.reportError(e);
  }
  return false;
}

function getMigratedData() {
#expand const MOZ_BUILD_APP = "__MOZ_BUILD_APP__";
#expand const MOZ_APP_NAME = "__MOZ_APP_NAME__";
  const MAX_MIGRATED_TYPES = 16;

  let bundle = Services.strings.createBundle("chrome://" + MOZ_BUILD_APP +
                                             "/locale/migration/migration.properties");

  // Loop over possible data to migrate to give the user a list of what will be preserved. This
  // assumes that if the string for the data exists, then it will be migrated because calling
  // getMigrateData now won't give the correct result.
  let dataTypes = [];
  for (let i = 1; i < MAX_MIGRATED_TYPES; ++i) {
    let itemID = Math.pow(2, i);
    try {
      let typeName = bundle.GetStringFromName(itemID + "_" + MOZ_APP_NAME);
      dataTypes.push(typeName);
    } catch (x) {
      // Catch exceptions when the string for a data type doesn't exist because it's not migrated
    }
  }
  return dataTypes;
}

function onResetProfileAccepted() {
  let retVals = window.arguments[0];
  retVals.reset = true;
}
