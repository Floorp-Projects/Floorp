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
 */

var bundle = srGetStrBundle("chrome://profile/locale/profileManager.properties");
var profileManagerMode = "selection";
var set = null;

// invoke the createProfile Wizard
function CreateProfileWizard()
{
  // Need to call CreateNewProfile xuls
  window.openDialog('chrome://profile/content/createProfileWizard.xul', 'CPW', 'chrome');
}

// update the display to show the additional profile
function CreateProfile( aProfName, aProfDir )
{
  AddItem( "profilekids", aProfName, aProfName );
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
    if( selected.getAttribute("rowMigrate") == "true" ) {
      // auto migrate if the user wants to
      var lString = bundle.GetStringFromName("migratebeforerename");
      lString = lString.replace(/\s*<html:br\/>/g,"\n");
      if( confirm( lString ) ) 
        profile.migrateProfile( profilename, true );
      else
        return false;
    }
    else {
      var oldName = selected.getAttribute("rowName");
      var newName = prompt( bundle.GetStringFromName("renameprofilepromptA") + oldName + bundle.GetStringFromName("renameprofilepromptB"), "" );
      if( newName == "" || !newName )
        return false;
        
      var migrate = selected.getAttribute("rowMigrate");
      dump("*** oldName = "+ oldName+ ", newName = "+ newName+ ", migrate = "+ migrate+ "\n");
      try {
        profile.renameProfile(oldName, newName);
        selected.firstChild.firstChild.setAttribute( "value", newName );
        selected.setAttribute( "rowName", newName );
        selected.setAttribute( "profile_name", newName );
      }
      catch (ex) {
        var lString = bundle.GetStringFromName("renamefailed");
        lString = lString.replace(/\s*<html:br\/>/g,"\n");
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

  if( selected.getAttribute("rowMigrate") == "true" ) {
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
    captionLine = bundle.GetStringFromName( "pm_title" );   // get manager's caption
    manageButton = bundle.GetStringFromName( "pm_button" ); // get the manage button caption
    buttonDisplay = "display: inherit;";                    // display the manager's buttons
    profileManagerMode = "manager";                         // swap the mode
    PersistAndLoadElements( selItems );                     // save the selection and load elements
  } 
  else {
    prattleIndex = 0;
    captionLine = bundle.GetStringFromName( "ps_title" );
    manageButton = bundle.GetStringFromName( "ps_button" ); 
    buttonDisplay = "display: none;";
    profileManagerMode = "selection";
    PersistAndLoadElements( selItems );
  }

  // swap deck  
  var deck = document.getElementById( "prattle" );
  deck.setAttribute( "index", prattleIndex )
    
  // update the manager button.
  var manage = document.getElementById( "manage" );
  manage.setAttribute( "value", manageButton );
    
  // swap caption
  ChangeCaption( captionLine );
  
  // display the management buttons
  var buttons = document.getElementById( "managementbox" );
  buttons.setAttribute( "style", buttonDisplay );

  // switch set
  if( set )
    set = false;
  else
    set = true;
}

// save which elements are selected in this mode and load elements into other mode,
// then reselect selected elements
function PersistAndLoadElements( aSelItems )
{
  // persist the profiles that are selected;
  var profileTree = document.getElementById("profiles");
  var selItemNodes = profileTree.selectedItems;    
  for( var i = 0; i < selItemNodes.length; i++ )
  {
    aSelItems[i] = selItemNodes[i].getAttribute("profile_name");
  }
  loadElements();                                         // reload list of profiles
  for( var i = 0; i < aSelItems.length; i++ )
  {
    var item = document.getElementsByAttribute("profile_name", aSelItems[i]);
    if( item.length ) {
      var item = item[0];
      profileTree.addItemToSelection( item );
    }
  }
}

// change the title of the profile manager/selection window.
function ChangeCaption( aCaption )
{
  var caption = document.getElementById( "caption" );
  while( caption.hasChildNodes() )
  {
    caption.removeChild( caption.firstChild );
  }
  newCaption = document.createTextNode( aCaption );
  caption.appendChild( newCaption );
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
  if( aEvent.clickCount == 2 ) {
    if( aEvent.target.nodeName.toLowerCase() == "treecell" )
      return onStart(); 
  }
  else 
    return showSelection(aEvent.target.parentNode.parentNode);
}