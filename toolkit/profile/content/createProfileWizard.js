/*
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
 *   Ben Goodger (30/09/99)
 *   Brant Gurganus (23/03/03)
 *   Stefan Borggraefe (17/10/03)
 *   Benjamin Smedberg <bsmedberg@covad.net> - 8-Apr-2004
 */ 

const C = Components.classes;
const I = Components.interfaces;

const ToolkitProfileService = "@mozilla.org/toolkit/profile-service;1";

var gProfileService;
var gProfileManagerBundle;

var gDefaultProfileParent;
var gOldProfileName;

// The directory where the profile will be created.
var gProfileRoot;

// Text node to display the location and name of the profile to create.
var gProfileDisplay;

// Called once when the wizard is opened.
function initWizard()
{ 
  try {
    gProfileService = C[ToolkitProfileService].getService(I.nsIToolkitProfileService);
    gProfileManagerBundle = document.getElementById("bundle_profileManager");

    var dirService = C["@mozilla.org/file/directory_service;1"].getService(I.nsIProperties);
    gDefaultProfileParent = dirService.get("DefProfRt", I.nsIFile);

    gOldProfileName = document.getElementById("profileName").value;

    // Initialize the profile location display.
    gProfileDisplay = document.getElementById("profileDisplay").firstChild;
    setDisplayToDefaultFolder();
  }
  catch(e) {
    window.close();
    throw (e);
  }
}

// Called every time the second wizard page is displayed.
function initSecondWizardPage() 
{
  var profileName = document.getElementById("profileName");
  profileName.select();
  profileName.focus();

  // Initialize profile name validation.
  checkCurrentInput(profileName.value);
}

function setDisplayToDefaultFolder()
{
  var defaultProfileDir = gDefaultProfileParent.clone();
  defaultProfileDir.append(document.getElementById("profileName").value);
  gProfileRoot = defaultProfileDir;
  document.getElementById("useDefault").disabled = true;
}

function updateProfileDisplay()
{
  gProfileDisplay.data = gProfileRoot.path;
}

// Invoke a folder selection dialog for choosing the directory of profile storage.
function chooseProfileFolder()
{
  var newProfileRoot;
  
  var dirChooser = C["@mozilla.org/filepicker;1"].createInstance(I.nsIFilePicker);
  dirChooser.init(window, gProfileManagerBundle.getString("chooseFolder"),
                  I.nsIFilePicker.modeGetFolder);
  dirChooser.appendFilters(I.nsIFilePicker.filterAll);
  dirChooser.show();
  newProfileRoot = dirChooser.file;

  // Disable the "Default Folder..." button when the default profile folder
  // was selected manually in the File Picker.
  document.getElementById("useDefault").disabled =
    (newProfileRoot.parent.equals(gDefaultProfileParent));

  gProfileRoot = newProfileRoot;
  updateProfileDisplay();
}

// Checks the current user input for validity and triggers an error message accordingly.
function checkCurrentInput(currentInput)
{
  var finishButton = document.documentElement.getButton("finish");
  var finishText = document.getElementById("finishText");
  var canAdvance;

  var errorMessage = checkProfileName(currentInput);

  if (!errorMessage) {
    finishText.className = "";
    finishText.firstChild.data = gProfileManagerBundle.getString("profileFinishText");
    canAdvance = true;
  }
  else {
    finishText.className = "error";
    finishText.firstChild.data = errorMessage;
    canAdvance = false;
  }

  document.documentElement.canAdvance = canAdvance;
  finishButton.disabled = !canAdvance;

  updateProfileDisplay();
}

function updateProfileName(aNewName) {
  checkCurrentInput(aNewName);

  if (gProfileRoot.leafName == gOldProfileName) {
    gProfileRoot.leafName = aNewName;
    updateProfileDisplay();
  }
  gOldProfileName = aNewName;
}

// Checks whether the given string is a valid profile name.
// Returns an error message describing the error in the name or "" when it's valid.
function checkProfileName(profileNameToCheck)
{
  // Check for emtpy profile name.
  if (!/\S/.test(profileNameToCheck))
    return gProfileManagerBundle.getString("profileNameEmpty");

  // Check whether all characters in the profile name are allowed.
  if (/([\\*:?<>|\/\"])/.test(profileNameToCheck))
    return gProfileManagerBundle.getFormattedString("invalidChar", [RegExp.$1]);

  // Check whether a profile with the same name already exists.
  if (profileExists(profileNameToCheck))
    return gProfileManagerBundle.getString("profileExists");

  // profileNameToCheck is valid.
  return "";
}

function profileExists(aName)
{
  var profiles = gProfileService.profiles;
  while (profiles.hasMoreElements()) {
    var profile = profiles.getNext().QueryInterface(I.nsIToolkitProfile);
    if (profile.name.toLowerCase() == aName.toLowerCase())
      return true;
  }

  return false;
}

// Called when the first wizard page is shown.
function enableNextButton()
{
  document.documentElement.canAdvance = true;
}

function onFinish() 
{
  var profileName = document.getElementById("profileName").value;
  var profile;

  // Create profile named profileName in profileRoot.
  try {
    profile = gProfileService.createProfile(gProfileRoot, profileName);
  }
  catch (e) {
    var profileCreationFailed =
      gProfileManagerBundle.getString("profileCreationFailed");
    var profileCreationFailedTitle =
      gProfileManagerBundle.getString("profileCreationFailedTitle");
    var promptService = C["@mozilla.org/embedcomp/prompt-service;1"].
      getService(I.nsIPromptService);
    promptService.alert(window, profileCreationFailedTitle,
                        profileCreationFailed + "\n" + e);

    return false;
  }

  // window.opener is false if the Create Profile Wizard was opened from the
  // command line.
  if (window.opener) {
    // Add new profile to the list in the Profile Manager.
    window.opener.CreateProfile(profile);
  }
  else {
    // Use the newly created Profile.
    var profileLock = profile.lock();

    var dialogParams = window.arguments[0].QueryInterface(I.nsIDialogParamBlock);
    dialogParams.objects.insertElementAt(profileLock, 0, false);
  }

  // Exit the wizard.
  return true;
}
