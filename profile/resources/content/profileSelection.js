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
 */

var selected    = null;
var currProfile = "";
var profile     = Components.classes["component://netscape/profile/manager"].createInstance();
if (profile)
  profile       = profile.QueryInterface(Components.interfaces.nsIProfile);
var bundle      = null;
var unset       = true;

function StartUp()
{
  SetUpOKCancelButtons();
  centerWindowOnScreen();
  if(window.location && window.location.search && window.location.search == "?manage=true" )
    SwitchProfileManagerMode();
  // bundle = srGetStrBundle("chrome://profile/locale/profileSelection.properties");
  loadElements();
  DoEnabling();
}

// function : <profileSelection.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem( aChildren, aCells, aIdfier, aMigrate)
{
  var kids    = document.getElementById(aChildren);
  var item    = document.createElement("treeitem");
  var row     = document.createElement("treerow");
  var cell    = document.createElement("treecell");
  cell.setAttribute("value",aCells);
  cell.setAttribute("align","left");
  if( aMigrate )
    cell.setAttribute("class", "dimmed-lowcontrast" );
  row.appendChild(cell);
  item.appendChild(row);
  item.setAttribute("profile_name", aIdfier);
  item.setAttribute("rowName", aIdfier);
  if( aMigrate )
    item.setAttribute("rowMigrate", "true");
  // 23/10/99 - no roaming access yet!
  //  var roaming = document.getElementById("roamingitem");
  //  kids.insertBefore(item,roaming);
  kids.appendChild(item);
}

// function : <profileSelection.js>::loadElements();
// purpose  : load profiles into tree
function loadElements()
{
  var profileList   = "";
  profileList       = profile.getProfileList();
  profileList       = profileList.split(",");

  try {
    currProfile = profile.currentProfile;
  }
  catch (ex) {
    if (profileList != "")
      currProfile = profileList;
  }

  // remove existing nodes...
  var profilekids = document.getElementById( "profilekids" );
  while( profilekids.hasChildNodes() )
  {
    profilekids.removeChild( profilekids.firstChild );
  }
  
  for (var i = 0; i < profileList.length; i++) {
    var pvals = profileList[i].split(" - ");
    // only add profiles that have been migrated
    if( profileManagerMode == "selection" && pvals[1] != "migrate" ) {
      AddItem( "profilekids", pvals[0], pvals[0], false );
    }
    else if( profileManagerMode == "manager" ){
      AddItem( "profilekids", pvals[0], pvals[0], ( pvals[1] == "migrate" ) );
    }
  }
}

// function : <profileSelection.js>::onStart();
// purpose  : starts mozilla given the selected profile (user choice: "Start Mozilla")
function onStart()
{
  var startButton = document.getElementById("ok");
  var profileTree = document.getElementById("profiles");
  if( startButton.getAttribute("disabled") == "true" ||
      profileTree.selectedItems.length != 1 )
    return;
    
  var selected = profileTree.selectedItems[0];
    
  var profilename = selected.getAttribute("profile_name");
  if( selected.getAttribute("rowMigrate") == "true" ) {
    if( confirm( bundle.GetStringFromName("migratebeforestart") ) ) 
      profile.migrateProfile( profilename, true );
    else
      return false;
  }
  try {
    dump("start with profile: " + profilename + "\n");
    profile.startApprunner(profilename);
    ExitApp();
  }
  catch (ex) {
    var stringA = bundle.GetStringFromName(failProfileStartA);
    var stringB = bundle.GetStringFromName(failProfileStartB);
    alert(stringA + " " + profilename + " " + stringB);
  }
}

// function : <profileSelection.js>::onExit();
// purpose  : quits startup process (User Choice: "Exit")
function onExit()
{
  try {
    profile.forgetCurrentProfile();
  }
  catch (ex) {
    dump("Failed to forget current profile.\n");
  }
  ExitApp();
}

// function : <profileSelection.js>::ExitApp();
// purpose  : halts startup process forcefully
function ExitApp()
{
  // Need to call this to stop the event loop
  var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService);
  appShell.Quit();
}

// function : <profileSelection.js>::showSelection();
// purpose  : selects the profile and enables the start button
function showSelection(node)
{
  DoEnabling();
}

// function : <profileSelection.js>::startWithProfile();
// purpose  : starts mozilla with selected profile automatically on dblclick
function startWithProfile(node)
{
  selected = node;
  onStart();
}

function onManageProfiles()
{
  window.openDialog("chrome://profile/content/profileManager.xul","","chrome");
}

function foo()
{
  if( !set ) {
    if( profileManagerMode == "manager" )
      oldCaptionManager = document.getElementById( "caption" ).firstChild.nodeValue;
    else
      oldCaptionSelection = document.getElementById( "caption" ).firstChild.nodeValue;
    ChangeCaption( "What Is Mozollia?" ); // DO NOT LOCALIZE!
    set = true;
  } 
  else {
    var tempCaption = document.getElementById( "caption" ).firstChild.nodeValue;
    if( profileManagerMode == "manager" ) {
      ChangeCaption( oldCaptionManager );
      oldCaptionManager = tempCaption;
    }
    else {
      ChangeCaption( oldCaptionSelection )
      oldCaptionSelection = tempCaption;
    }
    set = false;
  }
}

function SetUpOKCancelButtons()
{
  doSetOKCancel( onStart, onExit, null, null );
  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");
  okButton.setAttribute( "value", bundle.GetStringFromName("startButton") );
  okButton.setAttribute( "class", ( okButton.getAttribute("class") + " padded" ) );
  cancelButton.setAttribute( "value", bundle.GetStringFromName("exitButton") );
}
