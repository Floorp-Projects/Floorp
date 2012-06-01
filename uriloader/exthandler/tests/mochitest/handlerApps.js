/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      "http://mochi.test:8888/tests/uriloader/exthandler/tests/mochitest/" + 
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

  // if we get this far without an exception, we've at least partly passed
  // (remaining check in handlerApp.xhtml)
  ok(true, "webHandler launchWithURI (existing window/tab) started");

  // make the web browser launch in its own window/tab
  webHandler.launchWithURI(uri);
  
  // if we get this far without an exception, we've passed
  ok(true, "webHandler launchWithURI (new window/tab) test started");

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
    var exe = windowsDir.clone().QueryInterface(Components.interfaces.nsILocalFile);
    exe.appendRelativePath("SYSTEM32\\HOSTNAME.EXE");

  } else if (osString == "Darwin") { 
    var localAppsDir = dirSvc.getFile("LocApp", {});
    exe = localAppsDir.clone();
    exe.append("iCal.app"); // lingers after the tests finish, but this seems
                            // seems better than explicitly killing it, since
                            // developers who run the tests locally may well
                            // information in their running copy of iCal

    if (navigator.userAgent.match(/ SeaMonkey\//)) {
      // SeaMonkey tinderboxes don't like to have iCal lingering (and focused)
      // on next test suite run(s).
      todo(false, "On SeaMonkey, testing OS X as generic Unix. (Bug 749872)");

      // assume a generic UNIX variant
      exe = Components.classes["@mozilla.org/file/local;1"].
            createInstance(Components.interfaces.nsILocalFile);
      exe.initWithPath("/bin/echo");
    }
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
