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

var bundle = srGetStrBundle("chrome://profile/locale/profileManager.properties");
var profileManagerMode = "selection";
var set = null;

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  // Need to call CreateNewProfile xuls
  window.openDialog('chrome://profile/content/createProfileWizard.xul', 'CPW', 'chrome,modal=yes');
}

// update the display to show the additional profile
function CreateProfile( aProfName, aProfDir )
{
  var profile = new Profile( aProfName, aProfDir, "yes" );
  AddItem( "profilekids", profile );
}

// rename the selected profile
function RenameProfile()
{
  renameButton = document.getElementById("renbutton");
  if( renameButton.getAttribute("disabled") == "true" )
    return;
  var profileTree = document.getElementById( "profiles" );
  if( !profileTree.selectedItems.length ) {
    alert( bundle.GetStringFromName("noneselectedrename") );
    return;
  }
  else if( profileTree.selectedItems.length != 1 ) {
    alert( bundle.GetStringFromName("wrongnumberselectedrename") );
    return;
  }
  else {
    var selected = profileTree.selectedItems[0];
    if( selected.getAttribute("rowMigrate") == "no" ) {
      // migrate if the user wants to
      var lString = bundle.GetStringFromName("migratebeforerename");
      lString = lString.replace(/\s*<html:br\/>/g,"\n");
      if( confirm( lString ) ) 
        profile.migrateProfile( profilename, true );
      else
        return false;
    }
    else {
      var oldName = selected.getAttribute("rowName");
      var newName = prompt( bundle.GetStringFromName("renameprofilepromptA") + oldName + bundle.GetStringFromName("renameprofilepromptB"), oldName );
      dump("*** newName = |" + newName + "|\n");
      if( newName == "" || !newName )
        return false;
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
        
      var migrate = selected.getAttribute("rowMigrate");
      dump("*** oldName = "+ oldName+ ", newName = "+ newName+ ", migrate = "+ migrate+ "\n");
      try {
        profile.renameProfile(oldName, newName);
        selected.firstChild.firstChild.setAttribute( "value", newName );
        selected.setAttribute( "rowName", newName );
        selected.setAttribute( "profile_name", newName );
      }
      catch(e) {
        var lString = bundle.GetStringFromName("profileExists");
        alert( lString );
      }
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
  if( !profileTree.selectedItems.length ) {
    alert( bundle.GetStringFromName( "noneselecteddelete" ) );
    return;
  }
  else if( profileTree.selectedItems.length != 1 ) {
    alert( bundle.GetStringFromName("wrongnumberselectedrename") );
    return;
  }

  var selected = profileTree.selectedItems[0];
  var name = selected.getAttribute("rowName");

  if( selected.getAttribute("rowMigrate") == "no" ) {
      // auto migrate if the user wants to. THIS IS REALLY REALLY DUMB PLEASE FIX THE BACK END.
    var lString = bundle.GetStringFromName("migratebeforedelete");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    if( confirm( lString ) ) 
      profile.migrateProfile( profilename, true );
    else
      return false;
  }

  var win = window.openDialog('chrome://profile/content/deleteProfile.xul', 'Deleter', 'chrome,modal=yes');
  return win;
}

// Delete the profile, with the delete flag set as per instruction above.
function DeleteProfile( deleteFiles )
{
  var profileTree = document.getElementById( "profiles" );
  var profileKids = document.getElementById( "profilekids" )
  var selected = profileTree.selectedItems[0];
  var name = selected.getAttribute( "rowName" );
  try {
    profile.deleteProfile( name, deleteFiles );
    profileKids.removeChild( selected );
  }
  catch (ex) {
    var lString = bundle.GetStringFromName("deletefailed");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    alert( lString );
  }
  // set the button state
  DoEnabling();
}

function ConfirmMigrateAll()
{
  if( confirm( bundle.GetStringFromName("migrateallprofiles") ) )
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
      captionLine = "Manage Profiles Yah";
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
      captionLine = "Select Profile Yah";
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
  switch( aEvent.which ) 
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
  if( aEvent.clickCount == 2 && aEvent.which == 1 ) {
    if( aEvent.target.nodeName.toLowerCase() == "treecell" && 
        aEvent.target.parentNode.parentNode.nodeName.toLowerCase() != "treehead" )
      return onStart(); 
  }
  else 
    return showSelection(aEvent.target.parentNode.parentNode);
}
