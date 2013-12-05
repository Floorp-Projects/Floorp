/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// handlerApp.xhtml grabs this for verification purposes via window.opener
var testURI = "webcal://127.0.0.1/rheeeeet.html";

const Cc = SpecialPowers.Cc;

function test() {

  const isOSXMtnLion = navigator.userAgent.indexOf("Mac OS X 10.8") != -1;
  const isOSXMavericks = navigator.userAgent.indexOf("Mac OS X 10.9") != -1;
  if (isOSXMtnLion || isOSXMavericks) {
    todo(false, "This test fails on OS X 10.8 and 10.9, see bug 786938");
    SimpleTest.finish();
    return;
  }

  // set up the web handler object
  var webHandler = Cc["@mozilla.org/uriloader/web-handler-app;1"].
    createInstance(SpecialPowers.Ci.nsIWebHandlerApp);
  webHandler.name = "Test Web Handler App";
  webHandler.uriTemplate =
      "http://mochi.test:8888/tests/uriloader/exthandler/tests/mochitest/" + 
      "handlerApp.xhtml?uri=%s";
  
  // set up the uri to test with
  var ioService = Cc["@mozilla.org/network/io-service;1"].
    getService(SpecialPowers.Ci.nsIIOService);
  var uri = ioService.newURI(testURI, null, null);

  // create a window, and launch the handler in it
  var newWindow = window.open("", "handlerWindow", "height=300,width=300");
  var windowContext = 
    SpecialPowers.wrap(newWindow).QueryInterface(SpecialPowers.Ci.nsIInterfaceRequestor).
    getInterface(SpecialPowers.Ci.nsIWebNavigation).
    QueryInterface(SpecialPowers.Ci.nsIDocShell);
 
  webHandler.launchWithURI(uri, windowContext); 

  // if we get this far without an exception, we've at least partly passed
  // (remaining check in handlerApp.xhtml)
  ok(true, "webHandler launchWithURI (existing window/tab) started");

  // make the web browser launch in its own window/tab
  webHandler.launchWithURI(uri);
  
  // if we get this far without an exception, we've passed
  ok(true, "webHandler launchWithURI (new window/tab) test started");

  // set up the local handler object
  var localHandler = Cc["@mozilla.org/uriloader/local-handler-app;1"].
    createInstance(SpecialPowers.Ci.nsILocalHandlerApp);
  localHandler.name = "Test Local Handler App";
  
  // get a local app that we know will be there and do something sane
  var osString = Cc["@mozilla.org/xre/app-info;1"].
                 getService(SpecialPowers.Ci.nsIXULRuntime).OS;

  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(SpecialPowers.Ci.nsIDirectoryServiceProvider);
  if (osString == "WINNT") {
    var windowsDir = dirSvc.getFile("WinD", {});
    var exe = windowsDir.clone().QueryInterface(SpecialPowers.Ci.nsILocalFile);
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
      exe = Cc["@mozilla.org/file/local;1"].
            createInstance(SpecialPowers.Ci.nsILocalFile);
      exe.initWithPath("/bin/echo");
    }
  } else {
    // assume a generic UNIX variant
    exe = Cc["@mozilla.org/file/local;1"].
          createInstance(SpecialPowers.Ci.nsILocalFile);
    exe.initWithPath("/bin/echo");
  }

  localHandler.executable = exe;
  localHandler.launchWithURI(ioService.newURI(testURI, null, null));

  // if we get this far without an exception, we've passed
  ok(true, "localHandler launchWithURI test");

  // if we ever decide that killing iCal is the right thing to do, change 
  // the if statement below from "NOTDarwin" to "Darwin"
  if (osString == "NOTDarwin") {

    var killall = Cc["@mozilla.org/file/local;1"].
                  createInstance(SpecialPowers.Ci.nsILocalFile);
    killall.initWithPath("/usr/bin/killall");
  
    var process = Cc["@mozilla.org/process/util;1"].
                  createInstance(SpecialPowers.Ci.nsIProcess);
    process.init(killall);
    
    var args = ['iCal'];
    process.run(false, args, args.length);
  }

  SimpleTest.waitForExplicitFinish();
}

test();
