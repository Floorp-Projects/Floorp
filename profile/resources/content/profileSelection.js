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
 *   Ben Goodger (28/10/99)
 *   Seth Spitzer (28/10/99)
 */

var selected    = null;
var currProfile = "";
var profile     = Components.classes["component://netscape/profile/manager"].createInstance();
if(profile)
  profile       = profile.QueryInterface(Components.interfaces.nsIProfile);             
var bundle      = null;
var unset       = true;

function StartUp()
{
 // bundle = srGetStrBundle("chrome://profile/locale/profileSelection.properties");
 loadElements();
}

// function : <profileSelection.js>::AddItem();
// purpose  : utility function for adding items to a tree.
function AddItem(aChildren,aCells,aIdfier)
{
  var kids    = document.getElementById(aChildren);
  var item    = document.createElement("treeitem");
  var row     = document.createElement("treerow");
  var cell    = document.createElement("treecell");
  var button  = document.createElement("titledbutton");
  button.setAttribute("value",aCells);
  button.setAttribute("class","displaybutton");
  button.setAttribute("align","left");
  cell.appendChild(button);
  row.appendChild(cell);
  item.appendChild(row);
  item.setAttribute("profile_name", aIdfier);
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

	for (var i = 0; i < profileList.length; i++)
	{
		var pvals = profileList[i].split(" - ");
		// only add profiles that have been migrated
		if (pvals[1] != "migrate") {
    			AddItem("profilekids",pvals[0],pvals[0]);
		}
	}
}

// function : <profileSelection.js>::onStart();
// purpose  : starts mozilla given the selected profile (user choice: "Start Mozilla")
function onStart()
{
	startButton = document.getElementById("start");
	if (startButton.getAttribute("disabled") == "true")
    return;
  if(!selected)
    return;
    
  	var profilename = selected.getAttribute("profile_name");

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

// function : <profileSelection.js>::setButtonsDisabledState();
// purpose  : sets the enabling of buttons
function setButtonsDisabledState(state)
{
	startButton = document.getElementById("start");
	startButton.setAttribute("disabled", state);
}

// function : <profileSelection.js>::showSelection();
// purpose  : selects the profile and enables the start button
function showSelection(node)
{
	// (see tree's onclick definition)
	// Tree events originate in the smallest clickable object which is the cell.  The object 
	// originating the event is available as event.target.  
	// We want the item in the
	// cell's row, so we got passed event.target.parentNode.parentNode
	selected = node;
  var tree = document.getElementById("profiles");
  if(tree.selectedItems.length > 0)
  	setButtonsDisabledState("false");
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
  var welcome = document.getElementById("welcometo");
  var mozilla = document.getElementById("mozilla");
  if(welcome.hasChildNodes() && mozilla.hasChildNodes()) {
    if(unset) {
      oldA = welcome.lastChild.nodeValue;
      oldB = mozilla.lastChild.nodeValue;
      welcome.removeChild(welcome.lastChild)
      var text = document.createTextNode("What Is");
      welcome.appendChild(text);
      mozilla.removeChild(mozilla.lastChild)
      var text = document.createTextNode("MOZOLLIA?");
      mozilla.appendChild(text);
      unset = false;
    } else {
      welcome.removeChild(welcome.lastChild)
      var text = document.createTextNode(oldA);
      welcome.appendChild(text);
      mozilla.removeChild(mozilla.lastChild)
      var text = document.createTextNode(oldB);
      mozilla.appendChild(text);
      unset = true;
    }
  }
}
