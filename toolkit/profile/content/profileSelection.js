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

function startup() {
  try {
    gDialogParams = window.arguments[0].
      QueryInterface(I.nsIDialogParamBlock);

    gProfileService = C[ToolkitProfileService].getService(I.nsIToolkitProfileService);

    gProfileManagerBundle = document.getElementById("bundle_profileManager");
    gBrandBundle = document.getElementById("bundle_brand");

    document.documentElement.centerWindowOnScreen();

    var profilesElement = document.getElementById("profiles");

    var profileList = gProfileService.profiles;
    while (profileList.hasMoreElements()) {
      var profile = profileList.getNext().QueryInterface(I.nsIToolkitProfile);

      var listitem = profilesElement.appendItem(profile.name, "");

      var tooltiptext =
        gProfileManagerBundle.getFormattedString("profileTooltip", [profile.name, profile.rootDir.path]);
      listitem.setAttribute("tooltiptext", tooltiptext);
      listitem.setAttribute("class", "listitem-iconic");
      listitem.profile = profile;
      try {
        if (profile === gProfileService.selectedProfile) {
          setTimeout(function(a) {
            profilesElement.ensureElementIsVisible(a);
            profilesElement.selectItem(a);
          }, 0, listitem);
        }
      } catch (e) { }
    }

    var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
    autoSelectLastProfile.checked = gProfileService.startWithLastProfile;
    profilesElement.focus();
  } catch (e) {
    window.close();
    throw (e);
  }
}

function acceptDialog() {
  var appName = gBrandBundle.getString("brandShortName");

  var profilesElement = document.getElementById("profiles");
  var selectedProfile = profilesElement.selectedItem;
  if (!selectedProfile) {
    var pleaseSelectTitle = gProfileManagerBundle.getString("pleaseSelectTitle");
    var pleaseSelect =
      gProfileManagerBundle.getFormattedString("pleaseSelect", [appName]);
    Services.prompt.alert(window, pleaseSelectTitle, pleaseSelect);

    return false;
  }

  var profileLock;

  try {
    profileLock = selectedProfile.profile.lock({ value: null });
  } catch (e) {
    if (!selectedProfile.profile.rootDir.exists()) {
      var missingTitle = gProfileManagerBundle.getString("profileMissingTitle");
      var missing =
        gProfileManagerBundle.getFormattedString("profileMissing", [appName]);
      Services.prompt.alert(window, missingTitle, missing);
      return false;
    }

    var lockedTitle = gProfileManagerBundle.getString("profileLockedTitle");
    var locked =
      gProfileManagerBundle.getFormattedString("profileLocked2", [appName, selectedProfile.profile.name, appName]);
    Services.prompt.alert(window, lockedTitle, locked);

    return false;
  }
  gDialogParams.objects.insertElementAt(profileLock.nsIProfileLock, 0, false);

  gProfileService.selectedProfile = selectedProfile.profile;
  gProfileService.defaultProfile = selectedProfile.profile;
  updateStartupPrefs();

  gDialogParams.SetInt(0, 1);

  gDialogParams.SetString(0, selectedProfile.profile.name);

  return true;
}

function exitDialog() {
  updateStartupPrefs();

  return true;
}

function updateStartupPrefs() {
  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  gProfileService.startWithLastProfile = autoSelectLastProfile.checked;

  /* Bug 257777 */
  gProfileService.startOffline = document.getElementById("offlineState").checked;
}

// handle key event on listboxes
function onProfilesKey(aEvent) {
  switch ( aEvent.keyCode ) {
  case KeyEvent.DOM_VK_BACK_SPACE:
    if (AppConstants.platform != "macosx")
      break;
  case KeyEvent.DOM_VK_DELETE:
    ConfirmDelete();
    break;
  case KeyEvent.DOM_VK_F2:
    RenameProfile();
    break;
  }
}

function onProfilesDblClick(aEvent) {
  if (aEvent.target.localName == "listitem")
    document.documentElement.acceptDialog();
}

// invoke the createProfile Wizard
function CreateProfileWizard() {
  window.openDialog('chrome://mozapps/content/profile/createProfileWizard.xul',
                    '', 'centerscreen,chrome,modal,titlebar', gProfileService);
}

/**
 * Called from createProfileWizard to update the display.
 */
function CreateProfile(aProfile) {
  var profilesElement = document.getElementById("profiles");

  var listitem = profilesElement.appendItem(aProfile.name, "");

  var tooltiptext =
    gProfileManagerBundle.getFormattedString("profileTooltip", [aProfile.name, aProfile.rootDir.path]);
  listitem.setAttribute("tooltiptext", tooltiptext);
  listitem.setAttribute("class", "listitem-iconic");
  listitem.profile = aProfile;

  profilesElement.ensureElementIsVisible(listitem);
  profilesElement.selectItem(listitem);
}

// rename the selected profile
function RenameProfile() {
  var profilesElement = document.getElementById("profiles");
  var selectedItem = profilesElement.selectedItem;
  if (!selectedItem) {
    return false;
  }

  var selectedProfile = selectedItem.profile;

  var oldName = selectedProfile.name;
  var newName = {value: oldName};

  var dialogTitle = gProfileManagerBundle.getString("renameProfileTitle");
  var msg =
    gProfileManagerBundle.getFormattedString("renameProfilePrompt", [oldName]);

  if (Services.prompt.prompt(window, dialogTitle, msg, newName, null, {value:0})) {
    newName = newName.value;

    // User hasn't changed the profile name. Treat as if cancel was pressed.
    if (newName == oldName)
      return false;

    try {
      selectedProfile.name = newName;
    } catch (e) {
      var alTitle = gProfileManagerBundle.getString("profileNameInvalidTitle");
      var alMsg = gProfileManagerBundle.getFormattedString("profileNameInvalid", [newName]);
      Services.prompt.alert(window, alTitle, alMsg);
      return false;
    }

    selectedItem.label = newName;
    var tiptext = gProfileManagerBundle.
                  getFormattedString("profileTooltip",
                                     [newName, selectedProfile.rootDir.path]);
    selectedItem.setAttribute("tooltiptext", tiptext);

    return true;
  }

  return false;
}

function ConfirmDelete() {
  var profileList = document.getElementById( "profiles" );

  var selectedItem = profileList.selectedItem;
  if (!selectedItem) {
    return false;
  }

  var selectedProfile = selectedItem.profile;
  var deleteFiles = false;

  if (selectedProfile.rootDir.exists()) {
    var dialogTitle = gProfileManagerBundle.getString("deleteTitle");
    var dialogText =
      gProfileManagerBundle.getFormattedString("deleteProfileConfirm",
                                               [selectedProfile.rootDir.path]);

    var buttonPressed = Services.prompt.confirmEx(window, dialogTitle, dialogText,
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0) +
                          (Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1) +
                          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_2),
                          gProfileManagerBundle.getString("dontDeleteFiles"),
                          null,
                          gProfileManagerBundle.getString("deleteFiles"),
                          null, {value:0});
    if (buttonPressed == 1)
      return false;

    if (buttonPressed == 2)
      deleteFiles = true;
  }

  selectedProfile.remove(deleteFiles);
  profileList.removeChild(selectedItem);
  if (profileList.firstChild != undefined) {
    profileList.selectItem(profileList.firstChild);
  }

  return true;
}
