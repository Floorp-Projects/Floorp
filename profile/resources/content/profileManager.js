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

var commonDialogService = nsJSComponentManager.getService("component://netscape/appshell/commonDialogs",
                                                          "nsICommonDialogs");
var bundle = srGetStrBundle("chrome://communicator/locale/profile/profileManager.properties");
var brandBundle = srGetStrBundle("chrome://global/locale/brand.properties");
var profileManagerMode = "selection";
var set = null;

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  // Need to call CreateNewProfile xuls
  window.openDialog('chrome://communicator/content/profile/createProfileWizard.xul', 'CPW', 'chrome,modal=yes,titlebar=yes');
}

// update the display to show the additional profile
function CreateProfile( aProfName, aProfDir )
{
  var profile = new Profile( aProfName, aProfDir, "yes" );
  var item = AddItem( "profilekids", profile );
  var profileTree = document.getElementById( "profiles" );
  if( item )
    profileTree.selectItem( item );
}

// rename the selected profile
function RenameProfile()
{
  renameButton = document.getElementById("renbutton");
  if( renameButton.getAttribute("disabled") == "true" )
    return;
  var profileTree = document.getElementById( "profiles" );
  var selected = profileTree.selectedItems[0];
  if( selected.firstChild.firstChild.getAttribute("rowMigrate") == "no" ) {
    // migrate if the user wants to
    var lString = bundle.GetStringFromName("migratebeforerename");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    lString = lString.replace(/%brandShortName%/, brandBundle.GetStringFromName("brandShortName"));
    var title = bundle.GetStringFromName("migratetitle");
    if (commonDialogService.Confirm(window, title, lString))
      profile.migrateProfile( profilename, true );
    else
      return false;
  }
  else {
    var oldName = selected.getAttribute("rowName");
    var result = { };
    var dialogTitle = bundle.GetStringFromName("renameprofiletitle");
    var msg = bundle.GetStringFromName("renameProfilePrompt");
    msg = msg.replace(/%oldProfileName%/gi, oldName);
    while (1) {
      var rv = commonDialogService.Prompt(window, dialogTitle, msg, oldName, result);
      if (rv) {
        var newName = result.value;
        if (!newName) return false;
        var invalidChars = ["/", "\\", "*", ":"];
        for( var i = 0; i < invalidChars.length; i++ )
        {
          if( newName.indexOf( invalidChars[i] ) != -1 ) {
            var aString = bundle.GetStringFromName("invalidCharA");
            var bString = bundle.GetStringFromName("invalidCharB");
            bString = bString.replace(/\s*<html:br\/>/g,"\n");
            var lString = aString + invalidChars[i] + bString;
            alert( lString );
            return false;
          }
        }
        
        var migrate = selected.firstChild.firstChild.getAttribute("rowMigrate");
        try {
          profile.renameProfile(oldName, newName);
          selected.firstChild.firstChild.setAttribute( "value", newName );
          selected.setAttribute( "rowName", newName );
          selected.setAttribute( "profile_name", newName );
        }
        catch(e) {
          var lString = bundle.GetStringFromName("profileExists");
          var profileExistsTitle = bundle.GetStringFromName("profileExistsTitle");
          commonDialogService.Alert(window, profileExistsTitle, lString);
          continue;
        }
      }
      break;
    }
  }
  // set the button state
  DoEnabling();  
}


function ConfirmDelete()
{
  deleteButton = document.getElementById("delbutton");
  if( deleteButton.getAttribute("disabled") == "true" )
    return;
  var profileTree = document.getElementById( "profiles" );

  var selected = profileTree.selectedItems[0];
  var name = selected.getAttribute("rowName");

  if( selected.firstChild.firstChild.getAttribute("rowMigrate") == "no" ) {
      // auto migrate if the user wants to. THIS IS REALLY REALLY DUMB PLEASE FIX THE BACK END.
    var lString = bundle.GetStringFromName("migratebeforedelete");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    lString = lString.replace(/%brandShortName%/, brandBundle.GetStringFromName("brandShortName"));
    var title = bundle.GetStringFromName("deletetitle");
    if (commonDialogService.Confirm(window, title, lString)) {
      profile.deleteProfile( name, false );
      var profileKids = document.getElementById( "profilekids" )
      profileKids.removeChild( selected );
    }
    return;
  }

  var win = window.openDialog('chrome://communicator/content/profile/deleteProfile.xul', 'Deleter', 'chrome,modal=yes,titlebar=yes');
  return win;
}

// Delete the profile, with the delete flag set as per instruction above.
function DeleteProfile( deleteFiles )
{
  var profileTree = document.getElementById( "profiles" );
  var profileKids = document.getElementById( "profilekids" )
  if (profileTree.selectedItems && profileTree.selectedItems.length) {
    var selected = profileTree.selectedItems[0];
    var firstAdjacent = selected.previousSibling;
    var name = selected.getAttribute( "rowName" );
    try {
      profile.deleteProfile( name, deleteFiles );
      profileKids.removeChild( selected );
    }
    catch (ex) {
    }
    if( firstAdjacent )
      profileTree.selectItem( firstAdjacent );
    // set the button state
    DoEnabling();
  }
}

function ConfirmMigrateAll()
{
  var string = bundle.GetStringFromName("migrateallprofiles");
  var title = bundle.GetStringFromName("migrateallprofilestitle");
  if (commonDialogService.Confirm(window, title, string))
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
      captionLine = bundle.GetStringFromName( "pm_title" );   // get manager's caption
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
      captionLine = bundle.GetStringFromName( "ps_title" );
    } catch(e) {
      captionLine = "Select Profile *";
    }
    profileManagerMode = "selection";
  }

  // swap deck  
  var deck = document.getElementById( "prattle" );
  deck.setAttribute( "index", prattleIndex )
    
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

// do button enabling based on tree selection
function DoEnabling()
{
  var renbutton = document.getElementById( "renbutton" );
  var delbutton = document.getElementById( "delbutton" );
  var start     = document.getElementById( "ok" );
  
  var profileTree = document.getElementById( "profiles" );
  var items = profileTree.selectedItems;
  if( items.length != 1 )
  {
    renbutton.setAttribute( "disabled", "true" );
    delbutton.setAttribute( "disabled", "true" );
    start.setAttribute( "disabled", "true" );
  }
  else {
    if( renbutton.getAttribute( "disabled" ) == "true" )
      renbutton.removeAttribute( "disabled", "true" );
    if( delbutton.getAttribute( "disabled" ) == "true" )
      delbutton.removeAttribute( "disabled", "true" );
    if( start.getAttribute( "disabled" ) == "true" )
      start.removeAttribute( "disabled", "true" );
  }
}

// handle key event on trees
function HandleKeyEvent( aEvent )
{
  switch( aEvent.keyCode ) 
  {
  case 13:
    onStart();
    break;
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
  if( aEvent.detail == 2 && aEvent.which == 1 ) {
    if( aEvent.target.nodeName.toLowerCase() == "treecell" && 
        aEvent.target.parentNode.parentNode.nodeName.toLowerCase() != "treehead" )
      return onStart(); 
  }
}
