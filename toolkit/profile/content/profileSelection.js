/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const C = Components.classes;
const I = Components.interfaces;

const ToolkitProfileService = "@mozilla.org/toolkit/profile-service;1";

var gDialogParams;
var gProfileManagerBundle;
var gBrandBundle;
var gProfileService;

function startup()
{
  gDialogParams = window.arguments[0].QueryInterface(I.nsIDialogParamBlock);

  gProfileService = C[ToolkitProfileService].getService(I.nsIToolkitProfileService);

  gProfileManagerBundle = document.getElementById("bundle_profileManager");
  gBrandBundle = document.getElementById("bundle_brand");

  document.documentElement.centerWindowOnScreen();

  var browser = document.getElementById("browser");
  browser.frameLoader.docShell.contentViewer.DOMDocument.defaultView
         .addEventListener('startbrowser', function() {
    if (acceptDialog()) {
      this.close();
    }
  }.bind(this), false);
}

function acceptDialog()
{
  var appName = gBrandBundle.getString("brandShortName");

  var selectedProfile = gProfileService.selectedProfile;
  if (!selectedProfile) {
    var pleaseSelectTitle = gProfileManagerBundle.getString("pleaseSelectTitle");
    var pleaseSelect =
      gProfileManagerBundle.getFormattedString("pleaseSelect", [appName]);
    Services.prompt.alert(window, pleaseSelectTitle, pleaseSelect);

    return false;
  }

  var profileLock;

  try {
    profileLock = selectedProfile.lock({ value: null });
  }
  catch (e) {
    if (!selectedProfile.rootDir.exists()) {
      var missingTitle = gProfileManagerBundle.getString("profileMissingTitle");
      var missing =
        gProfileManagerBundle.getFormattedString("profileMissing", [appName]);
      Services.prompt.alert(window, missingTitle, missing);
      return false;
    }

    var lockedTitle = gProfileManagerBundle.getString("profileLockedTitle");
    var locked =
      gProfileManagerBundle.getFormattedString("profileLocked2", [appName, selectedProfile.name, appName]);
    Services.prompt.alert(window, lockedTitle, locked);

    return false;
  }

  gDialogParams.objects.insertElementAt(profileLock.nsIProfileLock, 0, false);

  gDialogParams.SetInt(0, 1);
  gDialogParams.SetString(0, selectedProfile.name);

  return true;
}

function exitDialog()
{
  return true;
}
