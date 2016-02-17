/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla Automated Testing Code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bob Clary <bob@bclary.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
  From mozilla/toolkit/content
  These files did not have a license
*/

function canQuitApplication()
{
  var os = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  if (!os) 
  {
    return true;
  }

  try 
  {
    var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
      .createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", null);
    
    // Something aborted the quit process. 
    if (cancelQuit.data)
    {
      return false;
    }
  }
  catch (ex) 
  {
  }
  os.notifyObservers(null, "quit-application-granted", null);
  return true;
}

function goQuitApplication(waitForSafeBrowsing)
{
  const privs = 'UniversalPreferencesRead UniversalPreferencesWrite ' +
    'UniversalXPConnect';

  try
  {
    netscape.security.PrivilegeManager.enablePrivilege(privs);
  }
  catch(ex)
  {
    throw('goQuitApplication: privilege failure ' + ex);
  }

  var xulRuntime = Components.classes["@mozilla.org/xre/app-info;1"]
                 .getService(Components.interfaces.nsIXULRuntime);
  if (xulRuntime.processType == xulRuntime.PROCESS_TYPE_CONTENT) {
    // If we're running in a remote browser, emit an event for a
    // frame script to pick up to quit the whole browser.
    var event = new CustomEvent("TalosQuitApplication", {bubbles:true, detail: {waitForSafeBrowsing}});
    document.dispatchEvent(event);
    return;
  }

  if (waitForSafeBrowsing) {
    var SafeBrowsing = Components.utils.
      import("resource://gre/modules/SafeBrowsing.jsm", {}).SafeBrowsing;

    var whenDone = () => {
      goQuitApplication(false);
    };

    SafeBrowsing.addMozEntriesFinishedPromise.then(whenDone, whenDone);
    // Speed things up in case nobody else called this:
    SafeBrowsing.init();
    return false;
  }

  if (!canQuitApplication())
  {
    return false;
  }

  const kAppStartup = '@mozilla.org/toolkit/app-startup;1';
  const kAppShell   = '@mozilla.org/appshell/appShellService;1';
  var   appService;

  if (kAppStartup in Components.classes)
  {
    appService = Components.classes[kAppStartup].
      getService(Components.interfaces.nsIAppStartup);

  }
  else if (kAppShell in Components.classes)
  {
    appService = Components.classes[kAppShell].
      getService(Components.interfaces.nsIAppShellService);
  }
  else
  {
    throw 'goQuitApplication: no AppStartup/appShell';
  }

  var windowManager = Components.
    classes['@mozilla.org/appshell/window-mediator;1'].getService();

  var windowManagerInterface = windowManager.
    QueryInterface(Components.interfaces.nsIWindowMediator);

  var enumerator = windowManagerInterface.getEnumerator(null);

  while (enumerator.hasMoreElements())
  {
    var domWindow = enumerator.getNext();
    if (("tryToClose" in domWindow) && !domWindow.tryToClose())
    {
      return false;
    }
    domWindow.close();
  }

  try
  {
    appService.quit(appService.eForceQuit);
  }
  catch(ex)
  {
    throw('goQuitApplication: ' + ex);
  }

  return true;
}


