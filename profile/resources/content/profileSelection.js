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
 *   Ben Goodger (03/01/00)
 *   Seth Spitzer (28/10/99)
 *   Dan Veditz <dveditz@netscape.com>
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

  if (gStartupMode) {
    document.documentElement.setAttribute("buttonlabelcancel",
      document.documentElement.getAttribute("buttonlabelexit"));
    document.documentElement.setAttribute("buttonlabelaccept",
      document.documentElement.getAttribute("buttonlabelstart"));
  }

  if(window.location && window.location.search && window.location.search == "?manage=true" )
    SwitchProfileManagerMode();
  else {  
    // Set up the intro text, depending on our context
    var introTextItem = document.getElementById("intro");
    var insertText = document.documentElement.getAttribute("buttonlabelaccept");
    var introText = gProfileManagerBundle.getFormattedString(
                      gStartupMode ? "intro_start" : "intro_switch",
                      [insertText]);
    introTextItem.textContent = introText;
  }

  var dirServ = Components.classes['@mozilla.org/file/directory_service;1']
                          .getService(Components.interfaces.nsIProperties);

  // "AggRegF" stands for Application Registry File.
  // Forgive the weird name directory service has adapted for
  // application registry file....
  var regFile = dirServ.get("AppRegF", Components.interfaces.nsIFile);

  Registry = Components.classes['@mozilla.org/registry;1'].createInstance();
  Registry = Registry.QueryInterface(Components.interfaces.nsIRegistry);
  Registry.open(regFile);

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
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch);
  if (prefs.getBoolPref("profile.manage_only_at_launch"))
    autoSelectLastProfile.hidden = true;
  else
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
    var profileList = document.getElementById( "profiles" );
    var currentProfileItem = profileList.getElementsByAttribute("profile_name", currentProfile).item(0);
    if( currentProfileItem ) {
      var currentProfileIndex = profileList.view.getIndexOfItem(currentProfileItem);
      profileList.view.selection.select( currentProfileIndex );
      profileList.treeBoxObject.ensureRowIsVisible( currentProfileIndex );
    }
  }
  catch(e) {
    dump("*** failed to select current profile in list\n");
  }
}

// function : <profileSelection.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem(aName, aMigrated)
{
  var tree = document.getElementById("profiles");
  var treeitem = document.createElement("treeitem");
  var treerow = document.createElement("treerow");
  var treecell = document.createElement("treecell");
  treecell.setAttribute("label", aName);
  treecell.setAttribute("properties", "rowMigrate-" + aMigrated);
  treeitem.setAttribute("profile_name", aName);
  treeitem.setAttribute("rowMigrate", aMigrated);
  treerow.appendChild(treecell);
  treeitem.appendChild(treerow);
  tree.lastChild.appendChild(treeitem);
}

// function : <profileSelection.js>::loadElements();
// purpose  : load profiles into tree
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

      AddItem(node.name, migrated);

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
  var selected = profileList.view.getItemAtIndex(profileList.currentIndex);
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
  if (!autoSelectLastProfile.hidden)
    profile.startWithLastUsedProfile = autoSelectLastProfile.checked;
  
  try {
    profile.currentProfile = profilename;
  }
  catch (ex) {
	  var brandName = gBrandBundle.getString("brandShortName");    
    var message;
    var fatalError = false;
    switch (ex.result) {
      case Components.results.NS_ERROR_FILE_ACCESS_DENIED:
        message = gProfileManagerBundle.getFormattedString("profDirLocked", [brandName, profilename]);
        message = message.replace(/\s*<html:br\/>/g,"\n");
        break;
      case Components.results.NS_ERROR_FILE_NOT_FOUND:
        message = gProfileManagerBundle.getFormattedString("profDirMissing", [brandName, profilename]);
        message = message.replace(/\s*<html:br\/>/g,"\n");
        break;
      case Components.results.NS_ERROR_ABORT:
        message = gProfileManagerBundle.getFormattedString("profileSwitchFailed", [brandName, profilename, brandName, brandName]);
        message = message.replace(/\s*<html:br\/>/g,"\n");
        fatalError = true;
        break;
      default:
        message = ex.message;
        break;
  }
      promptService.alert(window, null, message);

      if (fatalError)
      {
        var appShellService = Components.classes["@mozilla.org/appshell/appShellService;1"]
                              .getService(Components.interfaces.nsIAppShellService);
        appShellService.quit(Components.interfaces.nsIAppShellService.eForceQuit);
      }

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
