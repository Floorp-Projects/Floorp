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

var profile = Components.classes["component://netscape/profile/manager"].createInstance();
profile = profile.QueryInterface(Components.interfaces.nsIProfile); 

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
  
  // instantiate the Wizard Manager
  wizardManager                   = new WizardManager( frame_id, null, null, wizardMap );
  wizardManager.URL_PagePrefix    = "chrome://profile/content/";
  wizardManager.URL_PagePostfix   = ".xul";

  // set the button handler functions
  wizardManager.SetHandlers( null, null, onFinish, onCancel, null, null );
  // load the start page
  dump("calling loadpage...\n");
  dump("startPage:: " + startPage + "\n");
	wizardManager.LoadPage( startPage, false );
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
    ExitApp();
  }
}

function onFinish()
{
  // check if we're at final stage 
  if( !wizardManager.wizardMap[wizardManager.currentPageTag].finish )
    return;

  var tag =  wizardManager.WSM.GetTagFromURL( wizardManager.content_frame.src, "/", ".xul" );
  wizardManager.WSM.SavePageData( tag, null, null, null );
  proceed = processCreateProfileData();
	if( proceed ) {
		if( window.opener ) {
			window.opener.CreateProfile();
			window.close();
		} 
    else 
      ExitApp();
	}
	else
		return;
}

/** void processCreateProfileData( void ) ;
 *  - purpose: 
 *  - in:  nothing
 *  - out: nothing
 **/               
function processCreateProfileData()
{
	//Process Create Profile Data
  var profName = wizardManager.WSM.PageData["newProfile1_2"].ProfileName.value;
  var profDir = wizardManager.WSM.PageData["newProfile1_2"].ProfileDir.value;
  
  try {
    // note: deleted check for empty profName string here as this should be
    //       done by panel. -bmg (31/10/99)
		// todo: move this check into the panel itself, activated ontyping :P
    //       this should definetly be moved to that page.. but how about providing
    //       user with some feedback about what's wrong. .. TOOLTIP! o_O
    //       or.. some sort of onblur notification. like a dialog then, or a 
    //       dropout layery thing. yeah. something like that to tell them when 
    //       it happens, not when the whole wizard is complete. blah. 
    if( profile.profileExists( profName ) )	{
			return false;
		}
		profile.createNewProfile( profName, profDir );
		profile.startApprunner( profName );
		return true;
  }
  catch(e) {
    dump("*** Failed to create a profile\n");
  }
}

/** void ExitApp( void ) ;
 *  - purpose: quits the application properly and finally, stops event loop
 *  - in:  nothing
 *  - out: nothing
 **/               
function ExitApp()
{
  // Need to call this to stop the event loop
  var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService);
  appShell.Quit();
}

// load string bundle
var bundle = srGetStrBundle("chrome://profile/locale/createProfileWizard.properties");

