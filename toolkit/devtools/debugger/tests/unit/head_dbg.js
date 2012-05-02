/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource:///modules/devtools/dbg-server.jsm");
Cu.import("resource:///modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/Services.jsm");


function check_except(func)
{
  try {
    func();
  } catch (e) {
    do_check_true(true);
    return;
  }
  dump("Should have thrown an exception: " + func.toString());
  do_check_true(false);
}

function testGlobal(aName) {
  let systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
    .createInstance(Ci.nsIPrincipal);

  let sandbox = Cu.Sandbox(systemPrincipal);
  Cu.evalInSandbox("this.__name = '" + aName + "'", sandbox);
  return sandbox;
}

function addTestGlobal(aName)
{
  let global = testGlobal(aName);
  DebuggerServer.addTestGlobal(global);
  return global;
}

function getTestGlobalContext(aClient, aName, aCallback) {
  aClient.request({ "to": "root", "type": "listContexts" }, function(aResponse) {
    for each (let context in aResponse.contexts) {
      if (context.global == aName) {
        aCallback(context);
        return false;
      }
    }
    aCallback(null);
  });
}

function attachTestGlobalClient(aClient, aName, aCallback) {
  getTestGlobalContext(aClient, aName, function(aContext) {
    aClient.attachThread(aContext.actor, aCallback);
  });
}

function attachTestGlobalClientAndResume(aClient, aName, aCallback) {
  attachTestGlobalClient(aClient, aName, function(aResponse, aThreadClient) {
    aThreadClient.resume(function(aResponse) {
      aCallback(aResponse, aThreadClient);
    });
  })
}

/**
 * Initialize the testing debugger server.
 */
function initTestDebuggerServer()
{
  DebuggerServer.addActors("resource://test/testactors.js");
  DebuggerServer.init();
}

function finishClient(aClient)
{
  aClient.close(function() {
    do_test_finished();
  });
}

/**
 * Returns the full path of the file with the specified name in a
 * platform-independent and URL-like form.
 */
function getFilePath(aName)
{
  let file = do_get_file(aName);
  let path = Services.io.newFileURI(file).spec;
  let filePrePath = "file://";
  if ("nsILocalFileWin" in Ci &&
      file instanceof Ci.nsILocalFileWin) {
    filePrePath += "/";
  }
  return path.slice(filePrePath.length);
}
