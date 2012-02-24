/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

// based on onImportItemsPageShow from migration.js
function onResetProfileLoad() {
#expand const MOZ_BUILD_APP = "__MOZ_BUILD_APP__";
#expand const MOZ_APP_NAME = "__MOZ_APP_NAME__";
  const MAX_MIGRATED_TYPES = 16;

  var migratedItems = document.getElementById("migratedItems");
  var bundle = Services.strings.createBundle("chrome://" + MOZ_BUILD_APP +
                                             "/locale/migration/migration.properties");

  // Loop over possible data to migrate to give the user a list of what will be preserved. This
  // assumes that if the string for the data exists then it will be migrated since calling
  // getMigrateData now won't give the correct result.
  for (var i = 1; i < MAX_MIGRATED_TYPES; ++i) {
    var itemID = Math.pow(2, i);
    try {
      var checkbox = document.createElement("label");
      checkbox.setAttribute("value", bundle.GetStringFromName(itemID + "_" + MOZ_APP_NAME));
      migratedItems.appendChild(checkbox);
    } catch (x) {
      // Catch exceptions when the string for a data type doesn't exist because it's not migrated
    }
  }
}

function onResetProfileAccepted() {
  var retVals = window.arguments[0];
  retVals.reset = true;
}
