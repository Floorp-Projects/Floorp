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
 *   Ben Goodger <ben@netscape.com>
 */

//NOTE: This script expects gBrandBundle and gProfileManagerBundle to be
//      instanciated elsewhere (currently from StartUp in profileSelection.js)

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
var profileManagerMode = "selection";
var set = null;

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  // Need to call CreateNewProfile xuls
  window.openDialog('chrome://communicator/content/profile/createProfileWizard.xul', 'CPW', 'centerscreen,chrome,modal=yes,titlebar=yes');
}

// update the display to show the additional profile
function CreateProfile( aProfName, aProfDir )
{
  var profile = new Profile( aProfName, aProfDir, "yes" );
  var item = AddItem( "profiles", profile );
  var profileList = document.getElementById( "profiles" );
  if( item ) {
    profileList.selectItem( item );
    profileList.ensureElementIsVisible( item );
  }
}

// rename the selected profile
function RenameProfile()
{
  var lString, oldName, newName;
  renameButton = document.getElementById("renbutton");
  if (renameButton.getAttribute("disabled") == "true" )
    return false;
  var profileList = document.getElementById( "profiles" );
  var selected = profileList.selectedItems[0];
  var profilename = selected.getAttribute("profile_name");
  if( selected.getAttribute("rowMigrate") == "no" ) {
    // migrate if the user wants to
    lString = gProfileManagerBundle.getString("migratebeforerename");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    lString = lString.replace(/%brandShortName%/, gBrandBundle.getString("brandShortName"));
    var title = gProfileManagerBundle.getString("migratetitle");
    if (promptService.confirm(window, title, lString)) {
      var profileDir = profile.getProfileDir(profilename);
      if (profileDir) {
        profileDir = profileDir.QueryInterface( Components.interfaces.nsIFile );
        if (profileDir) {
          if (!profileDir.exists()) {
            var errorMessage = gProfileManagerBundle.getString("sourceProfileDirMissing");
            var profileDirMissingTitle = gProfileManagerBundle.getString("sourceProfileDirMissingTitle");
            promptService.alert(window, profileDirMissingTitle, errorMessage);
              return false;          
          }
        }
      }      
      profile.migrateProfile( profilename );
    }
    else
      return false;
  }
  else {
    oldName = selected.getAttribute("rowName");
    newName = {value:oldName};
    var dialogTitle = gProfileManagerBundle.getString("renameprofiletitle");
    var msg = gProfileManagerBundle.getString("renameProfilePrompt");
    msg = msg.replace(/%oldProfileName%/gi, oldName);
    while (1) {
      var rv = promptService.prompt(window, dialogTitle, msg, newName, null, {value:0});
      if (rv) {
        newName = newName.value;
        if (!newName) return false;
        var invalidChars = ["/", "\\", "*", ":"];
        for( var i = 0; i < invalidChars.length; i++ )
        {
          if( newName.indexOf( invalidChars[i] ) != -1 ) {
            var aString = gProfileManagerBundle.getString("invalidCharA");
            var bString = gProfileManagerBundle.getString("invalidCharB");
            bString = bString.replace(/\s*<html:br\/>/g,"\n");
            lString = aString + invalidChars[i] + bString;
            alert( lString );
            return false;
          }
        }
        
        var migrate = selected.getAttribute("rowMigrate");
        try {
          profile.renameProfile(oldName, newName);
          selected.setAttribute( "label", newName );
          selected.setAttribute( "rowName", newName );
          selected.setAttribute( "profile_name", newName );
        }
        catch(e) {
          lString = gProfileManagerBundle.getString("profileExists");
          var profileExistsTitle = gProfileManagerBundle.getString("profileExistsTitle");
          promptService.alert(window, profileExistsTitle, lString);
          continue;
        }
      }
      break;
    }
  }
  // set the button state
  DoEnabling(); 
  return true; 
}


function ConfirmDelete()
{
  deleteButton = document.getElementById("delbutton");
  if( deleteButton.getAttribute("disabled") == "true" )
    return;
  var profileList = document.getElementById( "profiles" );

  var selected = profileList.selectedItems[0];
  var name = selected.getAttribute("rowName");
  
  var dialogTitle = gProfileManagerBundle.getString("deletetitle");
  var dialogText;
  
  if( selected.getAttribute("rowMigrate") == "no" ) {
    var brandName = gBrandBundle.getString("brandShortName");
    dialogText = gProfileManagerBundle.getFormattedString("delete4xprofile", [brandName]);
    dialogText = dialogText.replace(/\s*<html:br\/>/g,"\n");
    
    if (promptService.confirm(window, dialogTitle, dialogText)) {
      profile.deleteProfile( name, false );
      profileList.removeChild( selected );
    }
    return;
  }
  else {
    var pathExists = true;
    try {
      var path = profile.getProfilePath(name);
    }
    catch (ex) {
      pathExists = false;
    }
    if (pathExists) {
      dialogText = gProfileManagerBundle.getFormattedString("deleteprofile", [path]);
      dialogText = dialogText.replace(/\s*<html:br\/>/g,"\n");
      var buttonPressed = promptService.confirmEx(window, dialogTitle, dialogText,
                              (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) +
                              (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1) +
                              (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2),
                              gProfileManagerBundle.getString("dontDeleteFiles"),
                              null,
                              gProfileManagerBundle.getString("deleteFiles"),
                              null, {value:0});
      if (buttonPressed != 1)
          DeleteProfile(buttonPressed == 2);
    }
    else
      DeleteProfile(false);
  }
}

// Delete the profile, with the delete flag set as per instruction above.
function DeleteProfile( deleteFiles )
{
  var profileList = document.getElementById( "profiles" );
  if (profileList.selectedItems && profileList.selectedItems.length) {
    var selected = profileList.selectedItems[0];
    var firstAdjacent = selected.previousSibling;
    var name = selected.getAttribute( "rowName" );
    try {
      profile.deleteProfile( name, deleteFiles );
      profileList.removeChild( selected );
    }
    catch (ex) {
    }
    if( firstAdjacent ) {
      profileList.selectItem( firstAdjacent );
      profileList.ensureElementIsVisible( firstAdjacent );
    }
    // set the button state
    DoEnabling();
  }
}

function ConfirmMigrateAll()
{
  var string = gProfileManagerBundle.getString("migrateallprofiles");
  var title = gProfileManagerBundle.getString("migrateallprofilestitle");
  if (promptService.confirm(window, title, string))
    return true;
  else 
    return false;
}

function SwitchProfileManagerMode()
{
  var prattleIndex  = null;
  var captionLine   = null;
  var buttonDisplay = null;
  var selItems      = [];
  
  if( profileManagerMode == "selection" )
  {
    prattleIndex = 1;                                       // need to switch to manager's index
    
    try {
      captionLine = gProfileManagerBundle.getString("pm_title"); // get manager's caption
    } catch(e) {
      captionLine = "Manage Profiles *";
    }
    
    var manage = document.getElementById( "manage" );       // hide the manage profiles button...
    var manageParent = manage.parentNode;
    manageParent.removeChild( manage );
    profileManagerMode = "manager";                         // swap the mode
  } 
  else {
    prattleIndex = 0;
    try {
      captionLine = gProfileManagerBundle.getString("ps_title");
    } catch(e) {
      captionLine = "Select Profile *";
    }
    profileManagerMode = "selection";
  }

  // swap deck  
  var deck = document.getElementById( "prattle" );
  deck.setAttribute( "selectedIndex", prattleIndex )
    
  // swap caption
  ChangeCaption( captionLine );
  
  set = !set;
}

// change the title of the profile manager/selection window.
function ChangeCaption( aCaption )
{
  var caption = document.getElementById( "header" );
  caption.setAttribute( "value", aCaption );
  window.title = aCaption;
}

// do button enabling based on listbox selection
function DoEnabling()
{
  var renbutton = document.getElementById( "renbutton" );
  var delbutton = document.getElementById( "delbutton" );
  var start     = document.getElementById( "ok" );
  
  var profileList = document.getElementById( "profiles" );
  var items = profileList.selectedItems;
  if( items.length != 1 )
  {
    renbutton.setAttribute( "disabled", "true" );
    delbutton.setAttribute( "disabled", "true" );
    start.setAttribute( "disabled", "true" );
  }
  else {
    if( renbutton.getAttribute( "disabled" ) == "true" )
      renbutton.removeAttribute( "disabled", "true" );
    if( start.getAttribute( "disabled" ) == "true" )
      start.removeAttribute( "disabled", "true" );
    
    var canDelete = true;
    if (!gStartupMode) {  
      var selected = profileList.selectedItems[0];
      var profileName = selected.getAttribute("profile_name");
      var currentProfile = profile.currentProfile;
      if (currentProfile && (profileName == currentProfile))
        canDelete = false;      
    }
    if (canDelete) {
      if ( delbutton.getAttribute( "disabled" ) == "true" )
        delbutton.removeAttribute( "disabled" );
    }
    else
      delbutton.setAttribute( "disabled", "true" );
      
  }
}

// handle key event on listboxes
function HandleKeyEvent( aEvent )
{
  switch( aEvent.keyCode ) 
  {
  case 46:
    if( profileManagerMode != "manager" )
      return;
    ConfirmDelete();
    break;
  case "VK_F2":
    if( profileManagerMode != "manager" )
      return;
    RenameProfile();
    break;
  }
}

function HandleClickEvent( aEvent )
{
  if( aEvent.detail == 2 && aEvent.button == 0 && aEvent.target.localName == "listitem") {
    if (!onStart())
      return false;
    window.close();
    return true;
  }
  return false;
}

