/*
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
 *   Ben Goodger (30/09/99)
 */ 

// The WIZARD of GORE

var profile = Components.classes["@mozilla.org/profile/manager;1"].getService();
profile = profile.QueryInterface(Components.interfaces.nsIProfileInternal); 
var gCreateProfileWizardBundle;
var gProfileManagerBundle;

// page specific variables
var profName      = "";
var profDir       = "";

// startup procedure
function Startup( startPage, frame_id )
{
  if( frame_id == "" ) {
    dump("Please supply a content_frame ID!");
    return false;
  }
  gCreateProfileWizardBundle = document.getElementById("bundle_createProfileWizard");
  gProfileManagerBundle = document.getElementById("bundle_profileManager");
  
  // move to center of screen if no opener, otherwise, to center of opener
  if( window.opener )
    moveToAlertPosition();
  else
    centerWindowOnScreen();
}

function onCancel()
{
  if( top.window.opener )
    window.close();
  else { 
    try {
      profile.forgetCurrentProfile();
    }
    catch (ex) {
      dump("failed to forget current profile.\n");
    }
    window.close();
  }
  return true;
}

function onFinish()
{
  var profName = document.getElementById("ProfileName").value;
  var profDir = document.getElementById("ProfileDir").getAttribute("rootFolder");
  var profLang = document.getElementById("ProfileLanguage").getAttribute("data");
  var profRegion = document.getElementById("ProfileRegion").getAttribute("data");

  // Get & select langcode
  var proceed = processCreateProfileData(profName, profDir, profLang, profRegion); 

  if( proceed ) {
      alert('fo');
    if( window.opener ) {
      window.opener.CreateProfile(profName, profDir);
    }
    else {
      profile.startApprunner(profName);
    }
    alert('fo');
    window.close();
  }
  return true;
}

// function createProfileWizard.js::chooseProfileFolder();
// invoke a folder selection dialog for choosing the directory of profile storage.
function chooseProfileFolder( aRootFolder )
{
  if( !aRootFolder ) {
    try {
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(Components.interfaces.nsIFilePicker);
      var newProfile1_2Bundle = document.getElementById("bundle_newProfile1_2");
      fp.init(window, newProfile1_2Bundle.getString("chooseFolder"), Components.interfaces.nsIFilePicker.modeGetFolder);
      fp.appendFilters(Components.interfaces.nsIFilePicker.filterAll);
      fp.show();
      // later change to 
      aRootFolder = fp.file.unicodePath;
    }
    catch(e) {
      aRootFolder = null;
    }
  }
  if( aRootFolder ) {
    var folderText = document.getElementById("ProfileDir");
    dump("*** setting rootFolderAttribute to " + aRootFolder + "\n");
    folderText.setAttribute( "rootFolder", aRootFolder );
    if ( aRootFolder != top.profile.defaultProfileParentDir.unicodePath )
      document.getElementById( "useDefault" ).removeAttribute("disabled");
    updateProfileName();
  }
}

/** void processCreateProfileData( void ) ;
 *  - purpose: 
 *  - in:  nothing
 *  - out: nothing
 **/               
function processCreateProfileData( aProfName, aProfDir, langcode, regioncode)
{
  try {
    // note: deleted check for empty profName string here as this should be
    //       done by panel. -bmg (31/10/99)
    // todo: move this check into the panel itself, activated ontyping :P
    //       this should definetly be moved to that page.. but how about providing
    //       user with some feedback about what's wrong. .. TOOLTIP! o_O
    //       or.. some sort of onblur notification. like a dialog then, or a 
    //       dropout layery thing. yeah. something like that to tell them when 
    //       it happens, not when the whole wizard is complete. blah. 
    if (profile.profileExists(aProfName)) {
      alert(gCreateProfileWizardBundle.getString("profileExists"));
      // this is a bad but probably acceptable solution for now. 
      // when we add more panels, we will want a better solution. 
      window.frames["content"].document.getElementById("ProfileName").focus();
      return false;
    }
    var invalidChars = ["/", "\\", "*", ":"];
    for( var i = 0; i < invalidChars.length; i++ )
    {
      if( aProfName.indexOf( invalidChars[i] ) != -1 ) {
        var aString = gProfileManagerBundle.getString("invalidCharA");
        var bString = gProfileManagerBundle.getString("invalidCharB");
        bString = bString.replace(/\s*<html:br\/>/g,"\n");
        var lString = aString + invalidChars[i] + bString;
        alert( lString );
        window.frames["content"].document.getElementById("ProfileName").focus();
        return false;
      }
    }

    // Adding code to see if the profile directory already exists....
    // XXXX - Further modifications like adding propmt dialog are required - XXXX
    var useExistingDir = false;
    var fileSpec = Components.classes["@mozilla.org/file/local;1"].createInstance();
    if ( fileSpec )
        fileSpec = fileSpec.QueryInterface( Components.interfaces.nsILocalFile );

    if (aProfDir == null)
        fileSpec.initWithUnicodePath(profile.defaultProfileParentDir.unicodePath);
    else
        fileSpec.initWithUnicodePath(aProfDir);

    fileSpec.appendUnicode(aProfName);

    if (fileSpec != null && fileSpec.exists())
      useExistingDir = true;

    dump("*** going to create a new profile called " + aProfName + " in folder: " + aProfDir + "\n");
    profile.createNewProfileWithLocales(aProfName, aProfDir, langcode, regioncode, useExistingDir);

    return true;
  }
  catch(e) {
    dump("*** Failed to create a profile\n");
  }
}
function updateProfileName()
{
  const nsILocalFile = Components.interfaces.nsILocalFile; 
  const nsILocalFile_CONTRACTID = "@mozilla.org/file/local;1";

  var profileName = document.getElementById( "ProfileName" );
  var folderDisplayElement = document.getElementById( "ProfileDir" );
  var rootFolder = folderDisplayElement.getAttribute( "rootFolder" );
  try {

    var sfile = Components.classes[nsILocalFile_CONTRACTID].createInstance(nsILocalFile); 
    if ( sfile ) {
      // later change to 
      sfile.initWithUnicodePath(rootFolder);
    }
    // later change to 
    sfile.appendUnicode(profileName.value);
    
    clearFolderDisplay();
    // later change to 
    var value = document.createTextNode( sfile.unicodePath );
    folderDisplayElement.appendChild( value );
  }
  catch(e) {
  }
}
function clearFolderDisplay()
{
  var folderText = document.getElementById("ProfileDir");
  if ( folderText.hasChildNodes() ) {
    while ( folderText.hasChildNodes() )
      folderText.removeChild( folderText.firstChild );
  }
}
function setDisplayToDefaultFolder()
{
  var profileName = document.getElementById( "ProfileName" );
  var profileDisplay = document.getElementById( "ProfileDir" );
  
  var fileSpec;
  try {
    fileSpec = top.profile.defaultProfileParentDir; 
    if ( fileSpec )
      fileSpec = fileSpec.QueryInterface( Components.interfaces.nsIFile );
    if ( fileSpec )
      profileDisplay.setAttribute("rootFolder", fileSpec.unicodePath );


  }
  catch(e) {
  }
  
  document.getElementById("useDefault").setAttribute("disabled", "true");
  
  // reset the display field
  updateProfileName();
}

function showLangDialog()
{
  var selectedLanguage = document.getElementById("ProfileLanguage").getAttribute("data");
  var selectedRegion = document.getElementById("ProfileRegion").getAttribute("data");
  var selectLang = window.openDialog("chrome://communicator/content/profile/selectLang.xul","","modal=yes,titlebar,resizable=no", selectedLanguage, selectedRegion);
}

// check to see if some user specified profile folder exists, otherwise use
// default. 
function initPage2()
{
  var displayField = document.getElementById( "ProfileDir" );
  if ( !displayField.value || !displayField.rootFolder )
    setDisplayToDefaultFolder();
}
