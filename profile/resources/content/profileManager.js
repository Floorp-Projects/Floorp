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
 *   Ben Goodger <ben@netscape.com>
 *   Brant Gurganus <brantgurganus2001@cherokeescouting.org>
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

//NOTE: This script expects gBrandBundle and gProfileManagerBundle to be
//      instanciated elsewhere (currently from StartUp in profileSelection.js)

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
var profileManagerMode = "selection";
var set = null;

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  window.openDialog('chrome://communicator/content/profile/createProfileWizard.xul',
                    '', 'centerscreen,chrome,modal,titlebar');
}

// update the display to show the additional profile
function CreateProfile( aProfName, aProfDir )
{
  AddItem(aProfName, "yes");
  var profileList = document.getElementById( "profiles" );
  profileList.view.selection.select(profileList.view.rowCount - 1);
  profileList.treeBoxObject.ensureRowIsVisible(profileList.currentIndex);
}

// rename the selected profile
function RenameProfile()
{
  var lString, oldName, newName;
  var renameButton = document.getElementById("renbutton");
  if (renameButton.getAttribute("disabled") == "true" )
    return false;
  var profileList = document.getElementById( "profiles" );
  var selected = profileList.view.getItemAtIndex(profileList.currentIndex);
  var profilename = selected.getAttribute("profile_name");
  var errorMessage = null;
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
            errorMessage = gProfileManagerBundle.getString("sourceProfileDirMissing");
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
    oldName = selected.getAttribute("profile_name");
    newName = {value:oldName};
    var dialogTitle = gProfileManagerBundle.getString("renameprofiletitle");
    var msg = gProfileManagerBundle.getString("renameProfilePrompt");
    msg = msg.replace(/%oldProfileName%/gi, oldName);
    while (1) {
      var rv = promptService.prompt(window, dialogTitle, msg, newName, null, {value:0});
      if (rv) {
        newName = newName.value;

        // User hasn't changed the profile name. Treat as if cancel was pressed.
        if (newName == oldName)
          return false;

        errorMessage = checkProfileName(newName);
        if (errorMessage) {
          var profileNameInvalidTitle = gProfileManagerBundle.getString("profileNameInvalidTitle");
          promptService.alert(window, profileNameInvalidTitle, errorMessage);
          return false;
        }
        
        var migrate = selected.getAttribute("rowMigrate");
        try {
          profile.renameProfile(oldName, newName);
          selected.firstChild.firstChild.setAttribute( "label", newName );
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
  var deleteButton = document.getElementById("delbutton");
  if( deleteButton.getAttribute("disabled") == "true" )
    return;
  var profileList = document.getElementById( "profiles" );

  var selected = profileList.view.getItemAtIndex(profileList.currentIndex);
  var name = selected.getAttribute("profile_name");
  
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
function DeleteProfile(deleteFiles)
{
  var profileList = document.getElementById("profiles");
  if (profileList.view.selection.count) {
    var selected = profileList.view.getItemAtIndex(profileList.currentIndex);
    var name = selected.getAttribute("profile_name");
    var previous = profileList.currentIndex - 1;

    try {
      profile.deleteProfile(name, deleteFiles);
      profileList.lastChild.removeChild(selected);

      if (previous) {
        profileList.view.selection.select(previous);
        profileList.treeBoxObject.ensureRowIsVisible(previous);
      }

      // set the button state
      DoEnabling();
    }
    catch (ex) {
      dump("Exception during profile deletion.\n");
    }
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
    
    var profileList = document.getElementById("profiles");
    profileList.focus();

    // hide the manage profiles button...
    document.documentElement.getButton("extra2").hidden = true;
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
  document.title = aCaption;
}

// do button enabling based on tree selection
function DoEnabling()
{
  var renbutton = document.getElementById( "renbutton" );
  var delbutton = document.getElementById( "delbutton" );
  var start     = document.documentElement.getButton( "accept" );
  
  var profileList = document.getElementById( "profiles" );
  if (profileList.view.selection.count == 0)
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
      var selected = profileList.view.getItemAtIndex(profileList.currentIndex);
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

// handle key event on tree
function HandleKeyEvent( aEvent )
{
  switch( aEvent.keyCode ) 
  {
  case 46:
    if( profileManagerMode != "manager" )
      return;
    ConfirmDelete();
    break;
  case KeyEvent.DOM_VK_F2:
    if( profileManagerMode != "manager" )
      return;
    RenameProfile();
    break;
  }
}

function HandleClickEvent( aEvent )
{
  if (aEvent.button == 0 && aEvent.target.parentNode.view.selection.count) {
    if (!onStart())
      return false;
    window.close();
    return true;
  }
  return false;
}

