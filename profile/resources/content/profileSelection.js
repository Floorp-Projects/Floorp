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
 *   Seth Spitzer (28/10/99)
 */

var selected    = null;
var currProfile = "";
var profile     = Components.classes["component://netscape/profile/manager"].createInstance();
if (profile)
  profile       = profile.QueryInterface(Components.interfaces.nsIProfile);
var unset       = true;

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var Registry;
var REGISTRY_NAMESPACE_URI = 'urn:mozilla-registry:'
var REGISTRY_VALUE_PREFIX = REGISTRY_NAMESPACE_URI + 'value:';
var kRegistry_Subkeys = RDF.GetResource(REGISTRY_NAMESPACE_URI + 'subkeys');

function StartUp()
{
  SetUpOKCancelButtons();
  centerWindowOnScreen();
  if(window.location && window.location.search && window.location.search == "?manage=true" )
    SwitchProfileManagerMode();

  Registry = Components.classes['component://netscape/registry-viewer'].createInstance();
  Registry = Registry.QueryInterface(Components.interfaces.nsIRegistryDataSource);
  Registry.openDefaultRegistry();
  Registry = Registry.QueryInterface(Components.interfaces.nsIRDFDataSource);
  
  loadElements();
  highlightCurrentProfile();
  DoEnabling();
  document.getElementById("profiles").focus();
}

// select the last opened profile in the profile list
function highlightCurrentProfile()
{
  try {
    var currentProfile = profile.currentProfile;
    if( !currentProfile )
      return;
    var currentProfileItem = document.getElementById( ( "profileName_" + currentProfile ) );
    var profileList = document.getElementById( "profiles" );
    profileList.selectItem( currentProfileItem );
  }
  catch(e) {
    dump("*** failed to select current profile in list\n");
  }
}

// function : <profileSelection.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem( aChildren, aProfileObject )
{
  var kids    = document.getElementById(aChildren);
  var item    = document.createElement("treeitem");
  var row     = document.createElement("treerow");
  var cell    = document.createElement("treecell");
  cell.setAttribute("value", aProfileObject.mName );
  cell.setAttribute("align","left");
  row.appendChild(cell);
  item.appendChild(row);
  item.setAttribute("profile_name", aProfileObject.mName );
  item.setAttribute("rowName", aProfileObject.mName );
  item.setAttribute("id", ( "profileName_" + aProfileObject.mName ) );
  item.setAttribute("rowMigrate",  aProfileObject.mMigrated );
  item.setAttribute("directory", aProfileObject.mDir );
  // 23/10/99 - no roaming access yet!
  //  var roaming = document.getElementById("roamingitem");
  //  kids.insertBefore(item,roaming);
  kids.appendChild(item);
}

function Profile ( aName, aDir, aMigrated ) 
{
  this.mName       = aName ? aName : null;
  this.mDir        = aDir ? aDir : null;
  this.mMigrated   = aMigrated ? aMigrated : null;
}

// function : <profileSelection.js>::loadElements();
// purpose  : load profiles into tree
function loadElements()
{
  var profileSRC = RDF.GetResource( "urn:mozilla-registry:key:/Profiles" );
  var profileProperty = RDF.GetResource( "urn:mozilla-registry:subkeys" );
  var profiles = Registry.GetTargets( profileSRC, profileProperty, true );
  if( profiles )
    profiles = profiles.QueryInterface( Components.interfaces.nsISimpleEnumerator );
  while( profiles.HasMoreElements() )
  {
    var currProfile = profiles.GetNext().QueryInterface( Components.interfaces.nsIRDFResource );
    // begin BAD BAD BAD BAD BAD BAD HACK {{
    var profileName = currProfile.Value.substring( currProfile.Value.lastIndexOf("/") + 1 );
    // }} end BAD BAD BAD BAD BAD BAD HACK
    var arcs = Registry.ArcLabelsOut( currProfile )
    if( arcs ) 
      arcs = arcs.QueryInterface( Components.interfaces.nsISimpleEnumerator );
    while( arcs.HasMoreElements() ) 
    {
      var property = arcs.GetNext().QueryInterface( Components.interfaces.nsIRDFResource );
      if( property == kRegistry_Subkeys )
        continue;
      var propstr = property.Value.substring( REGISTRY_VALUE_PREFIX.length );
      var target = Registry.GetTarget( currProfile, property, true );
      var literal = target.QueryInterface( Components.interfaces.nsIRDFLiteral );
      if( literal ) 
        var targetstr = literal.Value;
      else {
        literal = target.QueryInterface( Components.interfaces.nsIRDFInt )
        if( literal ) 
          var targetstr = literal.Value;
      }
      if( propstr == "migrated" )
        var migrated = targetstr;
      else if( propstr == "directory" )
        var directory = targetstr;
    }
    var profile = new Profile( profileName, directory, migrated );
    AddItem( "profilekids", profile );
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
  if( selected.getAttribute("rowMigrate") == "no" ) {
    var lString = bundle.GetStringFromName("migratebeforestart");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    if( confirm( lString ) ) 
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
  
  var okButtonString;
  var cancelButtonString;
  
  try {
    okButtonString = bundle.GetStringFromName("startButton");
    cancelButtonString = bundle.GetStringFromName("exitButton");
  } catch (e) {
    okButtonString = "Start Yah";
    cancelButtonString = "Exit Yah";
  }
  
  okButton.setAttribute( "value", okButtonString );
  okButton.setAttribute( "class", ( okButton.getAttribute("class") + " padded" ) );
  cancelButton.setAttribute( "value", cancelButtonString );
}
