/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger (03/01/00)
 *   Seth Spitzer (28/10/99)
 *   Dan Veditz <dveditz@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 */

const C = Components.classes;
const I = Components.interfaces;

const ToolkitProfileService = "@mozilla.org/toolkit/profile-service;1";
const PromptService = "@mozilla.org/embedcomp/prompt-service;1";

var gDialogParams;
var gProfileManagerBundle;
var gBrandBundle;
var gProfileService;
var gPromptService;

function startup()
{
  try {
    gDialogParams = window.arguments[0].
      QueryInterface(I.nsIDialogParamBlock);

    gProfileService = C[ToolkitProfileService].getService(I.nsIToolkitProfileService);

    gProfileManagerBundle = document.getElementById("bundle_profileManager");
    gBrandBundle = document.getElementById("bundle_brand");

    gPromptService = C[PromptService].getService(I.nsIPromptService);

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
          profilesElement.selectedItem = listitem;
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
    gPromptService.alert(window, pleaseSelectTitle, pleaseSelect);

    return false;
  }

  var profileLock;

  try {
    profileLock = selectedProfile.profile.lock();
  }
  catch (e) {
    var lockedTitle = gProfileManagerBundle.getString("profileLockedTitle");
    var locked =
      gProfileManagerBundle.getFormattedString("profileLocked", [appName, selectedProfile.profile.name]);
    gPromptService.alert(window, lockedTitle, locked);

    return false;
  }
  gDialogParams.objects.insertElementAt(profileLock.nsIProfileLock, 0, false);

  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  gProfileService.startWithLastProfile = autoSelectLastProfile.checked;
  gProfileService.selectedProfile = selectedProfile.profile;

  /* Bug 257777 */
#ifndef MOZ_PHOENIX
  gProfileService.startOffline = document.getElementById("offlineState").checked;
#endif

  gDialogParams.SetInt(0, 1);

  return true;
}

// handle key event on listboxes
function onProfilesKey(aEvent)
{
  switch( aEvent.keyCode ) 
  {
  case 46:
    confirmDelete();
    break;
  case "VK_F2":
    renameProfile();
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

  if (gPromptService.prompt(window, dialogTitle, msg, newName, null, {value:0})) {
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
      gPromptService.alert(window, alTitle, alMsg);
      return false;
    }

    selectedItem.label = newName;
    var tooltiptext =
      gProfileManagerBundle.getFormattedString("profileTooltip", [newName, aProfile.rootDir.path]);
    listitem.setAttribute("tooltiptext", tooltiptext);

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
      gProfileManagerBundle.getFormattedString("deleteProfile",
                                               [selectedProfile.rootDir.path]);

    var buttonPressed = gPromptService.confirmEx(window, dialogTitle, dialogText,
                          (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_0) +
                          (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1) +
                          (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_2),
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
  selectedItem.parentNode.removeChild(selectedItem);

  return true;
}
