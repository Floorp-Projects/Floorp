/* ***** BEGIN LICENSE BLOCK *****
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
 *   Ben Goodger (30/09/99)
 *   Brant Gurganus (23/03/03)
 *   Stefan Borggraefe (17/10/03)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gProfile = Components.classes["@mozilla.org/profile/manager;1"].getService(Components.interfaces.nsIProfileInternal);
var gProfileManagerBundle;

// The directory where the profile will be created.
var gProfileRoot;

// Text node to display the location and name of the profile to create.
var gProfileDisplay;

// Called once when the wizard is opened.
function initWizard()
{ 
  gProfileManagerBundle = document.getElementById("bundle_profileManager");
    
  // Initialize the profile location display.
  gProfileDisplay = document.getElementById("profileDisplay").firstChild;
  setDisplayToDefaultFolder();
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
  setDisplayToFolder(gProfile.defaultProfileParentDir);
  document.getElementById("useDefault").disabled = true;
}

function setDisplayToFolder(profileRoot)
{
  var profileName = document.getElementById("profileName");
  profileName.focus();
  gProfileRoot = profileRoot;
}

function updateProfileDisplay()
{
  var currentProfileName = document.getElementById("profileName").value;
  var profilePathAndName = gProfileRoot.clone();

  profilePathAndName.append(currentProfileName);
  gProfileDisplay.data = profilePathAndName.path;
}

// Shows the Language/Region Selection dialog and updates the data-Attributes to 
// the selected values.
function showLangDialog()
{
  var languageCode = document.getElementById("profileLanguage").getAttribute("data");
  var regionCode = document.getElementById("profileRegion").getAttribute("data");
  window.openDialog("chrome://communicator/content/profile/selectLang.xul",
                    "", "centerscreen,modal,titlebar",
                    languageCode, regionCode);
}

// Invoke a folder selection dialog for choosing the directory of profile storage.
function chooseProfileFolder()
{
  var newProfileRoot;
  
  try {
    var dirChooser = Components.classes["@mozilla.org/filepicker;1"].createInstance(Components.interfaces.nsIFilePicker);
    dirChooser.init(window, gProfileManagerBundle.getString("chooseFolder"), Components.interfaces.nsIFilePicker.modeGetFolder);
    dirChooser.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
    if (dirChooser.show() == dirChooser.returnCancel)
      return;
    newProfileRoot = dirChooser.file;
  }
  catch(e) {
    // If something fails, change nothing.
    return;
  }

  // Disable the "Default Folder..." button when the default profile folder
  // was selected manually in the File Picker.
  document.getElementById("useDefault").disabled = (newProfileRoot.equals(gProfile.defaultProfileParentDir));

  setDisplayToFolder(newProfileRoot);
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
  if (gProfile.profileExists(profileNameToCheck))
    return gProfileManagerBundle.getString("profileExists");

  // profileNameToCheck is valid.
  return "";
}

// Called when the first wizard page is shown.
function enableNextButton()
{
  document.documentElement.canAdvance = true;
}

function onCancel()
{
  // window.opener is false if the Create Profile Wizard was opened from the command line.
  if (!window.opener)
    return true;

  try {
    gProfile.forgetCurrentProfile();
  }
  catch (ex) {
  }

  return true;
}

function onFinish() 
{
  var profileName = document.getElementById("profileName").value;
  var languageCode = document.getElementById("profileLanguage").getAttribute("data");
  var regionCode = document.getElementById("profileRegion").getAttribute("data");

  var proceed = processCreateProfileData(profileName, gProfileRoot, languageCode, regionCode);
  // Error on profile creation. Don't leave the wizard so the user can correct his input.
  if (!proceed)
    return false;

  // window.opener is false if the Create Profile Wizard was opened from the command line.
  if (window.opener)
    // Add new profile to the list in the Profile Manager.
    window.opener.CreateProfile(profileName, gProfileRoot);
  else
    // Use the newly created Profile.
    gProfile.currentProfile = profileName;

  // Exit the wizard.
  return true;
}

// Create profile named profileName in profileRoot.
function processCreateProfileData(profileName, profileRoot, languageCode, regionCode)
{
  try {
    var profileLocation = profileRoot.clone();
    profileLocation.append(profileName);
    gProfile.createNewProfileWithLocales(profileName, profileRoot.path, languageCode, regionCode, profileLocation.exists());

    return true;
  }
  catch (e) {
    var profileCreationFailed = gProfileManagerBundle.getString("profileCreationFailed");
    var profileCreationFailedTitle = gProfileManagerBundle.getString("profileCreationFailedTitle");
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
    promptService.alert(window, profileCreationFailedTitle, profileCreationFailed);

    return false;
  }
}
