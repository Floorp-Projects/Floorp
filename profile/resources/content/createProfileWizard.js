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

// Navigation Set for pages contained in wizard 
var wizardMap = {
  newProfile1_1: { previous: null,              next: "newProfile1_2",    finish: false },
  newProfile1_2: { previous: "newProfile1_1",   next: null,               finish: true },
}

// page specific variables
var profName      = "";
var profDir       = "";
var wizardManager = null;

// startup procedure
function Startup( startPage, frame_id )
{
  if( frame_id == "" ) {
    dump("Please supply a content_frame ID!");
    return false;
  }
  gCreateProfileWizardBundle = document.getElementById("bundle_createProfileWizard");
  gProfileManagerBundle = document.getElementById("bundle_profileManager");
  
  // instantiate the Wizard Manager
  wizardManager                   = new WizardManager( frame_id, null, null, wizardMap );
  wizardManager.URL_PagePrefix    = "chrome://communicator/content/profile/";
  wizardManager.URL_PagePostfix   = ".xul";

  // set the button handler functions
  wizardManager.SetHandlers( null, null, onFinish, onCancel, null, null );
  // load the start page
  wizardManager.LoadPage (startPage, false);
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
}

function onFinish()
{

  // check if we're at final stage 
  if( !wizardManager.wizardMap[wizardManager.currentPageTag].finish )
    return;

  var tag =  wizardManager.WSM.GetTagFromURL( wizardManager.content_frame.src, "/", ".xul" );
  wizardManager.WSM.SavePageData( tag, null, null, null );

  var profName = wizardManager.WSM.PageData["newProfile1_2"].ProfileName.value;
  var profDir = wizardManager.WSM.PageData["newProfile1_2"].ProfileDir.value;
  var profLang = wizardManager.WSM.PageData["newProfile1_2"].ProfileLanguage.value;
  var profRegion = wizardManager.WSM.PageData["newProfile1_2"].ProfileRegion.value;

  // Get & select langcode
  proceed = processCreateProfileData(profName, profDir, profLang, profRegion); 
  
  if( proceed ) {
    if( window.opener ) {
      window.opener.CreateProfile(profName, profDir);
    }
    else {
      profile.startApprunner(profName);
    }
    window.close();
  }
  else
    return;
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

