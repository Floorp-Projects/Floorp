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
 */

var gDialogParams;
var gStartupMode; // TRUE if we're being shown at app startup, FALSE if from menu.
var gProfileManagerBundle;
var gBrandBundle;
var profile     = Components.classes["@mozilla.org/profile/manager;1"].getService();
if (profile)
  profile       = profile.QueryInterface(Components.interfaces.nsIProfileInternal);

var Registry;

function StartUp()
{
  gDialogParams = window.arguments[0].
                    QueryInterface(Components.interfaces.nsIDialogParamBlock);
  gStartupMode = (gDialogParams.GetString(0) == "startup");
  
  gProfileManagerBundle = document.getElementById("bundle_profileManager");
  gBrandBundle = document.getElementById("bundle_brand");

  SetUpOKCancelButtons();
  centerWindowOnScreen();
  if(window.location && window.location.search && window.location.search == "?manage=true" )
    SwitchProfileManagerMode();
    
  // Set up the intro text, depending on our context
  var introTextItem = document.getElementById("intro");
  var introText, insertText;
  if (gStartupMode) {
    insertText = gProfileManagerBundle.getFormattedString("startButton",
                                    [gBrandBundle.getString("brandShortName")]);
    introText = gProfileManagerBundle.getFormattedString("intro_start",
                    [insertText]);
  }
  else {
    insertText = gProfileManagerBundle.getString("selectButton");
    introText = gProfileManagerBundle.getFormattedString("intro_switch",
                    [insertText]);
  }
  introTextItem.childNodes[0].nodeValue = introText;

  var dirServ = Components.classes['@mozilla.org/file/directory_service;1'].createInstance();
  dirServ = dirServ.QueryInterface(Components.interfaces.nsIProperties);

  // "AggRegF" stands for Application Registry File.
  // Forgive the weird name directory service has adapted for
  // application registry file....
  var regFile = dirServ.get("AppRegF", Components.interfaces.nsIFile);

  Registry = Components.classes['@mozilla.org/registry;1'].createInstance();
  Registry = Registry.QueryInterface(Components.interfaces.nsIRegistry);
  Registry.open(regFile);

  // get new profile registry & users location and dump it to console
  // to let users know about it.
  var regFolder = dirServ.get("AppRegD", Components.interfaces.nsIFile);
  dump("New location for profile registry and user profile directories is -> " + regFolder.path + "\n");

  loadElements();
  highlightCurrentProfile();

  var offlineState = document.getElementById("offlineState");
  if (gStartupMode) {
    // set the checkbox to reflect the current state.
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                      getService(Components.interfaces.nsIIOService);
    offlineState.checked = ioService.offline;
  }
  else {
    // hide the checkbox in this context
    offlineState.hidden = true;
  }

  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  autoSelectLastProfile.checked = profile.startWithLastUsedProfile;

  var profileList = document.getElementById("profiles");
  profileList.focus();

  DoEnabling();
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
    if( currentProfileItem ) {
      profileList.selectItem( currentProfileItem );
      profileList.ensureElementIsVisible( currentProfileItem );
    }
  }
  catch(e) {
    dump("*** failed to select current profile in list\n");
  }
}

// function : <profileSelection.js>::AddItem();
// purpose  : utility function for adding items to a listbox.
function AddItem( aChildren, aProfileObject )
{
  var kids    = document.getElementById(aChildren);
  var listitem = document.createElement("listitem");
  listitem.setAttribute("label", aProfileObject.mName );
  listitem.setAttribute("rowMigrate",  aProfileObject.mMigrated );
  listitem.setAttribute("class", "listitem-iconic");
  listitem.setAttribute("profile_name", aProfileObject.mName );
  listitem.setAttribute("rowName", aProfileObject.mName );
  listitem.setAttribute("id", ( "profileName_" + aProfileObject.mName ) );
  // 23/10/99 - no roaming access yet!
  //  var roaming = document.getElementById("roamingitem");
  //  kids.insertBefore(item,roaming);
  kids.appendChild(listitem);
  return listitem;
}

function Profile ( aName, aMigrated )
{
  this.mName       = aName ? aName : null;
  this.mMigrated   = aMigrated ? aMigrated : null;
}

// function : <profileSelection.js>::loadElements();
// purpose  : load profiles into listbox
function loadElements()
{
  try {
    var profileRoot = Registry.getKey(Registry.Common, "Profiles");
    var regEnum = Registry.enumerateSubtrees( profileRoot );

    // the registry class is old and uses sucky nsIEnumerator instead
    // of nsISimpleEnumerator. We'll rely on a blank profile name to
    // throw us out of the while loop for now.
    regEnum.first();
    while (true)
    {
      var node = regEnum.currentItem();
      node = node.QueryInterface(Components.interfaces.nsIRegistryNode);

      if ( node.name == "" )
        break;

      var migrated = Registry.getString( node.key, "migrated" );

      AddItem( "profiles", new Profile( node.name, migrated ) );

      regEnum.next();
    }
  }
  catch (e) {}
}


// function : <profileSelection.js>::onStart();
// purpose  : sets the current profile to the selected profile (user choice: "Start Mozilla")
function onStart()
{
  var profileList = document.getElementById("profiles");
  var selected = profileList.selectedItems[0];
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);

  var profilename = selected.getAttribute("profile_name");
  if( selected.getAttribute("rowMigrate") == "no" ) {
    var lString = gProfileManagerBundle.getString("migratebeforestart");
    lString = lString.replace(/\s*<html:br\/>/g,"\n");
    lString = lString.replace(/%brandShortName%/gi,
                              gBrandBundle.getString("brandShortName"));
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

  // start in online or offline mode
  if (gStartupMode) {
    var offlineState = document.getElementById("offlineState");
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                      getService(Components.interfaces.nsIIOService);
    if (offlineState.checked != ioService.offline)
      ioService.offline = offlineState.checked;
  }
  
  var autoSelectLastProfile = document.getElementById("autoSelectLastProfile");
  profile.startWithLastUsedProfile = autoSelectLastProfile.checked;
  
  try {
    profile.currentProfile = profilename;
  }
  catch (ex) {
	  var brandName = gBrandBundle.getString("brandShortName");    
    var message;
    switch (ex.result) {
      case Components.results.NS_ERROR_FILE_ACCESS_DENIED:
        message = gProfileManagerBundle.getFormattedString("profDirLocked", [brandName, profilename]);
        message = message.replace(/\s*<html:br\/>/g,"\n");
        break;
      case Components.results.NS_ERROR_FILE_NOT_FOUND:
        message = gProfileManagerBundle.getFormattedString("profDirMissing", [brandName, profilename]);
        message = message.replace(/\s*<html:br\/>/g,"\n");
        break;
      default:
        message = ex.message;
        break;
  }
      promptService.alert(window, null, message);
      return false;
  }
  
  gDialogParams.SetInt(0, 1); // 1 == success
    
  return true;
}

// function : <profileSelection.js>::onExit();
// purpose  : quits startup process (User Choice: "Exit")
function onExit()
{
  gDialogParams.SetInt(0, 0); // 0 == cancel
  return true;
}

function SetUpOKCancelButtons()
{
  doSetOKCancel( onStart, onExit, null, null );
  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");

  var okButtonString;
  var cancelButtonString;
  
  try {    
    if (gStartupMode) {
      okButtonString = gProfileManagerBundle.getFormattedString("startButton",
                                    [gBrandBundle.getString("brandShortName")]);
      cancelButtonString = gProfileManagerBundle.getString("exitButton");
    }
    else {
      okButtonString = gProfileManagerBundle.getString("selectButton");
      cancelButtonString = gProfileManagerBundle.getString("cancel");
    }
  } catch (e) {
    okButtonString = "Start Yah";
    cancelButtonString = "Exit Yah";
  }

  okButton.setAttribute( "label", okButtonString );
  okButton.setAttribute( "class", ( okButton.getAttribute("class") + " padded" ) );
  cancelButton.setAttribute( "label", cancelButtonString );
}
