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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
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

// handlerApp.xhtml grabs this for verification purposes via window.opener
var testURI = "webcal://127.0.0.1/rheeeeet.html";

function test() {

  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect'); 

  // set up the web handler object
  var webHandler = 
    Components.classes["@mozilla.org/uriloader/web-handler-app;1"].
    createInstance(Components.interfaces.nsIWebHandlerApp);
  webHandler.name = "Test Web Handler App";
  webHandler.uriTemplate =
      "http://localhost:8888/tests/uriloader/exthandler/tests/mochitest/" + 
      "handlerApp.xhtml?uri=%s";
  
  // set up the uri to test with
  var ioService = Components.classes["@mozilla.org/network/io-service;1"].
    getService(Components.interfaces.nsIIOService);
  var uri = ioService.newURI(testURI, null, null);

  // create a window, and launch the handler in it
  var newWindow = window.open("", "handlerWindow", "height=300,width=300");
  var windowContext = 
    newWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor). 
    getInterface(Components.interfaces.nsIWebNavigation).
    QueryInterface(Components.interfaces.nsIDocShell);
 
  webHandler.launchWithURI(uri, windowContext); 

  // if we get this far without an exception, we've passed
  ok(true, "webHandler launchWithURI test");


  // set up the local handler object
  var localHandler = 
    Components.classes["@mozilla.org/uriloader/local-handler-app;1"].
    createInstance(Components.interfaces.nsILocalHandlerApp);
  localHandler.name = "Test Local Handler App";
  
  // get a local app that we know will be there and do something sane
  var osString = Components.classes["@mozilla.org/xre/app-info;1"].
                 getService(Components.interfaces.nsIXULRuntime).OS;

  var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"].
               getService(Components.interfaces.nsIDirectoryServiceProvider);
  if (osString == "WINNT") {
    var windowsDir = dirSvc.getFile("WinD", {});
    var exe = windowsDir.clone();
    exe.appendRelativePath("SYSTEM32\\HOSTNAME.EXE");

  } else if (osString == "Darwin") { 
    var localAppsDir = dirSvc.getFile("LocApp", {});
    exe = localAppsDir.clone();
    exe.append("iCal.app"); // lingers after the tests finish, but this seems
                            // seems better than explicitly killing it, since
                            // developers who run the tests locally may well
                            // information in their running copy of iCal
  } else {
    // assume a generic UNIX variant
    exe = Components.classes["@mozilla.org/file/local;1"].
          createInstance(Components.interfaces.nsILocalFile);
    exe.initWithPath("/bin/echo");
  }

  localHandler.executable = exe;
  localHandler.launchWithURI(ioService.newURI(testURI, null, null));

  // if we get this far without an exception, we've passed
  ok(true, "localHandler launchWithURI test");

  // if we ever decide that killing iCal is the right thing to do, change 
  // the if statement below from "NOTDarwin" to "Darwin"
  if (osString == "NOTDarwin") {

    var killall = Components.classes["@mozilla.org/file/local;1"].
                  createInstance(Components.interfaces.nsILocalFile);
    killall.initWithPath("/usr/bin/killall");
  
    var process = Components.classes["@mozilla.org/process/util;1"].
                  createInstance(Components.interfaces.nsIProcess);
    process.init(killall);
    
    var args = ['iCal'];
    process.run(false, args, args.length);
  }

  SimpleTest.waitForExplicitFinish();
}

test();
