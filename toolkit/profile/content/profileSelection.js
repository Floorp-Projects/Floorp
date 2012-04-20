/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger (03/01/00)
 *   Seth Spitzer (28/10/99)
 *   Dan Veditz <dveditz@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      }
      catch(e) { }
    }

    var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
    autoSelectLastProfile.checked = gProfileService.startWithLastProfile;
    profilesElement.focus();
  }
  catch(e) {
    window.close();
    throw (e);
  }
}

function acceptDialog()
{
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
  }
  catch (e) {
    if (!selectedProfile.profile.localDir.exists()) {
      var missingTitle = gProfileManagerBundle.getString("profileMissingTitle");
      var missing =
        gProfileManagerBundle.getFormattedString("profileMissing", [appName]);
      gPromptService.alert(window, missingTitle, missing);
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
  updateStartupPrefs();

  gDialogParams.SetInt(0, 1);

  gDialogParams.SetString(0, selectedProfile.profile.name);

  return true;
}

function exitDialog()
{
  updateStartupPrefs();
  
  return true;
}

function updateStartupPrefs()
{
  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  gProfileService.startWithLastProfile = autoSelectLastProfile.checked;

  /* Bug 257777 */
  gProfileService.startOffline = document.getElementById("offlineState").checked;
}

// handle key event on listboxes
function onProfilesKey(aEvent)
{
  switch( aEvent.keyCode ) 
  {
  case KeyEvent.DOM_VK_DELETE:
    ConfirmDelete();
    break;
  case KeyEvent.DOM_VK_F2:
    RenameProfile();
    break;
  }
}

function onProfilesDblClick(aEvent)
{
  if(aEvent.target.localName == "listitem")
    document.documentElement.acceptDialog();
}

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  window.openDialog('chrome://mozapps/content/profile/createProfileWizard.xul',
                    '', 'centerscreen,chrome,modal,titlebar', gProfileService);
}

/**
 * Called from createProfileWizard to update the display.
 */
function CreateProfile(aProfile)
{
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
function RenameProfile()
{
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
    }
    catch (e) {
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

function ConfirmDelete()
{
  var deleteButton = document.getElementById("delbutton");
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
