/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Communicator Shared Utility Library
 * for shared application glue for the Communicator suite of applications
 **/

/**
 * Go into online/offline mode
 **/

const kIOServiceProgID = "@mozilla.org/network/io-service;1";
const kObserverServiceProgID = "@mozilla.org/observer-service;1";

function toggleOfflineStatus()
{
  var checkfunc;
  try {
    checkfunc = document.getElementById("offline-status").getAttribute('checkfunc');
  }
  catch (ex) {
    checkfunc = null;
  }

  var ioService = Components.classes[kIOServiceProgID]
                            .getService(Components.interfaces.nsIIOService);
  if (checkfunc) {
    if (!eval(checkfunc)) {
      // the pre-offline check function returned false, so don't go offline
      return;
    }
  }
  ioService.offline = !ioService.offline;
}

function setOfflineUI(offline)
{
  var broadcaster = document.getElementById("Communicator:WorkMode");
  if (!broadcaster) return;

  //Checking for a preference "network.online", if it's locked, disabling 
  // network icon and menu item
  var prefService = Components.classes["@mozilla.org/preferences-service;1"];
  prefService = prefService.getService();
  prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
  
  var prefBranch = prefService.getBranch(null);
  
  var offlineLocked = prefBranch.prefIsLocked("network.online"); 
  
  if (offlineLocked ) {
      broadcaster.setAttribute("disabled","true");
  }

  var bundle = srGetStrBundle("chrome://communicator/locale/utilityOverlay.properties");

  if (offline)
    {
      broadcaster.setAttribute("offline", "true");
      broadcaster.setAttribute("tooltiptext", bundle.GetStringFromName("offlineTooltip"));
      broadcaster.setAttribute("label", bundle.GetStringFromName("goonline"));
    }
  else
    {
      broadcaster.removeAttribute("offline");
      broadcaster.setAttribute("tooltiptext", bundle.GetStringFromName("onlineTooltip"));
      broadcaster.setAttribute("label", bundle.GetStringFromName("gooffline"));
    }
}

var goPrefWindow = 0;

function getBrowserURL() {

  try {
    var prefs = Components.classes["@mozilla.org/preferences;1"];
    if (prefs) {
      prefs = prefs.getService();
      if (prefs)
        prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
    }
    if (prefs) {
      var url = prefs.CopyCharPref("browser.chromeURL");
      if (url)
        return url;
    }
  } catch(e) {
  }
  return "chrome://navigator/content/navigator.xul";
}

function goPageSetup()
{
}

function goEditCardDialog(abURI, card, okCallback, abCardURI)
{
  window.openDialog("chrome://messenger/content/addressbook/abEditCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal,titlebar",
					  {abURI:abURI, card:card, okCallback:okCallback, abCardURI:abCardURI});
}

function goPreferences(containerID, paneURL, itemID)
{
  var resizable;
  var pref = Components.classes["@mozilla.org/preferences;1"].getService(Components.interfaces.nsIPref);
  try {
    // We are resizable ONLY if in box debugging mode, because in
    // this special debug mode it is often impossible to see the 
    // content of the debug panel in order to disable debug mode.
    resizable = pref.GetBoolPref("xul.debug.box");
  }
  catch (e) {
    resizable = false;
  }

  //check for an existing pref window and focus it; it's not application modal
  const kWindowMediatorContractID = "@mozilla.org/rdf/datasource;1?name=window-mediator";
  const kWindowMediatorIID = Components.interfaces.nsIWindowMediator;
  const kWindowMediator = Components.classes[kWindowMediatorContractID].getService(kWindowMediatorIID);
  var lastPrefWindow = kWindowMediator.getMostRecentWindow("mozilla:preferences");
  if (lastPrefWindow)
    lastPrefWindow.focus();
  else {
    var resizability = resizable ? "yes" : "no";
    var features = "chrome,titlebar,resizable=" + resizability;
    openDialog("chrome://communicator/content/pref/pref.xul","PrefWindow", 
               features, paneURL, containerID, itemID);
  }
}

function goToggleToolbar( id, elementID )
{
  var toolbar = document.getElementById( id );
  var element = document.getElementById( elementID );
  if ( toolbar )
  {
    var attribValue = toolbar.getAttribute("hidden") ;

    if ( attribValue == "true" )
    {
      toolbar.setAttribute("hidden", "false" );
      if ( element )
        element.setAttribute("checked","true")
    }
    else
    {
      toolbar.setAttribute("hidden", true );
      if ( element )
        element.setAttribute("checked","false")
    }
    document.persist(id, 'hidden');
    document.persist(elementID, 'checked');
  }
}


function goClickThrobber( urlPref )
{
  var url;
  try {
    var pref = Components.classes["@mozilla.org/preferences;1"].getService();
    if( pref )
    pref = pref.QueryInterface( Components.interfaces.nsIPref );
    url = pref.getLocalizedUnicharPref(urlPref);
  }

  catch(e) {
    url = null;
  }

  if ( url )
    openTopWin(url);
}


//No longer needed.  Rip this out since we are using openTopWin
function goHelpMenu( url )
{
  /* note that this chrome url should probably change to not have all of the navigator controls */
  /* also, do we want to limit the number of help windows that can be spawned? */
  window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
}


function openTopWin( url )
{
  /* note that this chrome url should probably change to not have all of the navigator controls,
     but if we do this we need to have the option for chrome controls because goClickThrobber()
     needs to use this function with chrome controls */
  /* also, do we want to limit the number of help windows that can be spawned? */
    if ((url == null) || (url == "")) return;

    // xlate the URL if necessary
    if (url.indexOf("urn:") == 0)
    {
        url = xlateURL(url);        // does RDF urn expansion
    }

    // avoid loading "", since this loads a directory listing
    if (url == "") {
        url = "about:blank";
    }

    var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

    var topWindowOfType = windowManagerInterface.getMostRecentWindow( "navigator:browser" );
    if ( topWindowOfType )
    {
        topWindowOfType.focus();
    topWindowOfType._content.location.href = url;
    }
    else
    {
        window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
    }
}

function goAboutDialog()
{
  var defaultAboutState = false;
  try {
    var pref = Components.classes["@mozilla.org/preferences;1"].getService();
    if( pref )
      pref = pref.QueryInterface( Components.interfaces.nsIPref );
    defaultAboutState = pref.GetBoolPref("browser.show_about_as_stupid_modal_window");
  }
  catch(e) {
    defaultAboutState = false;
  }
  if( defaultAboutState )
    window.openDialog("chrome:global/content/about.xul", "About", "modal,chrome,resizable=yes,height=450,width=550");
  else
    window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", 'about:' );
}

// update menu items that rely on focus
function goUpdateGlobalEditMenuItems()
{
  goUpdateCommand('cmd_undo');
  goUpdateCommand('cmd_redo');
  goUpdateCommand('cmd_cut');
  goUpdateCommand('cmd_copy');
  goUpdateCommand('cmd_paste');
  goUpdateCommand('cmd_selectAll');
  goUpdateCommand('cmd_delete');
}

// update menu items that rely on the current selection
function goUpdateSelectEditMenuItems()
{
  goUpdateCommand('cmd_cut');
  goUpdateCommand('cmd_copy');
  goUpdateCommand('cmd_delete');
}

// update menu items that relate to undo/redo
function goUpdateUndoEditMenuItems()
{
  goUpdateCommand('cmd_undo');
  goUpdateCommand('cmd_redo');
}

// update menu items that depend on clipboard contents
function goUpdatePasteMenuItems()
{
  goUpdateCommand('cmd_paste');
}

// This used to be BrowserNewEditorWindow in navigator.js
function NewEditorWindow(aPageURL)
{
  // Open editor window with blank page
  // Kludge to leverage openDialog non-modal!
  window.openDialog( "chrome://editor/content", "_blank", "chrome,all,dialog=no", "about:blank");
}

function NewEditorFromTemplate()
{
  // XXX not implemented
}

function NewEditorFromDraft()
{
  // XXX not implemented
}

// Any non-editor window wanting to create an editor with a URL
//   should use this instead of "window.openDialog..."
//  We must always find an existing window with requested URL
// (When calling from a dialog, "launchWindow" is dialog's "opener"
//   and we need a delay to let dialog close)
function editPage(url, launchWindow, delay)
{
  // User may not have supplied a window
  if (!launchWindow)
  {
    if (window)
    {
      launchWindow = window;
    }
    else
    {
      dump("No window to launch an editor from!\n");
      return;
    }
  }

  // if the current window is a browser window, then extract the current charset menu setting from the current 
  // document and use it to initialize the new composer window...

  var wintype = document.firstChild.getAttribute('windowtype');
  var charsetArg;

  if (launchWindow && (wintype == "navigator:browser"))
    charsetArg = "charset=" + launchWindow._content.document.characterSet;

  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  if (!windowManager) return;
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  if ( !windowManagerInterface ) return;
  var enumerator = windowManagerInterface.getEnumerator( "composer:html" );
  if ( !enumerator ) return;

  while ( enumerator.hasMoreElements() )
  {
    var window = windowManagerInterface.convertISupportsToDOMWindow( enumerator.getNext() );
    if ( window && window.editorShell)
    {
      if (window.editorShell.checkOpenWindowForURLMatch(url, window))
      {
        // We found an editor with our url
        window.focus();
        return;
      }
    }
  }

  // Create new Composer window
  if (delay)
    launchWindow.delayedOpenWindow("chrome://editor/content", "chrome,all,dialog=no", url, charsetArg);
  else
    launchWindow.openDialog("chrome://editor/content", "_blank", "chrome,all,dialog=no", url, charsetArg);
}

// function that extracts the filename from a url
function extractFileNameFromUrl(urlstr)
{
  if (!urlstr) return null;
  return urlstr.slice(urlstr.lastIndexOf( "/" )+1);
}

var offlineObserver = {
  Observe: function(subject, topic, state) {
    // sanity checks
    if (topic != "network:offline-status-changed") return;
    setOfflineUI(state == "offline");
  }
}

function utilityOnLoad(aEvent)
{
  var broadcaster = document.getElementById("Communicator:WorkMode");
  if (!broadcaster) return;

  var observerService = Components.classes[kObserverServiceProgID]
		          .getService(Components.interfaces.nsIObserverService);

  // crude way to prevent registering twice.
  try {
    observerService.RemoveObserver(offlineObserver, "network:offline-status-changed");
  }
  catch (ex) {
  }
  observerService.AddObserver(offlineObserver, "network:offline-status-changed");
  // make sure we remove this observer later
  addEventListener("unload",utilityOnUnload,false);

  // set the initial state
  var ioService = Components.classes[kIOServiceProgID]
		      .getService(Components.interfaces.nsIIOService);
  setOfflineUI(ioService.offline);
}

function utilityOnUnload(aEvent) 
{
  var observerService = Components.classes[kObserverServiceProgID]
			  .getService(Components.interfaces.nsIObserverService);
  observerService.RemoveObserver(offlineObserver, "network:offline-status-changed");
}

addEventListener("load",utilityOnLoad,true);
