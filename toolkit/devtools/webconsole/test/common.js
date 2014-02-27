/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const XHTML_NS = "http://www.w3.org/1999/xhtml";

Cu.import("resource://gre/modules/Services.jsm");

let devtools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
let WebConsoleUtils = devtools.require("devtools/toolkit/webconsole/utils").Utils;

let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                          .getService(Ci.nsIConsoleAPIStorage);

let {ConsoleServiceListener, ConsoleAPIListener} =
  devtools.require("devtools/toolkit/webconsole/utils");

function initCommon()
{
  //Services.prefs.setBoolPref("devtools.debugger.log", true);

  Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
}

function initDebuggerServer()
{
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
}

function connectToDebugger(aCallback)
{
  initCommon();
  initDebuggerServer();

  let transport = DebuggerServer.connectPipe();
  let client = new DebuggerClient(transport);

  let dbgState = { dbgClient: client };
  client.connect(aCallback.bind(null, dbgState));
}

function attachConsole(aListeners, aCallback, aAttachToTab)
{
  function _onAttachConsole(aState, aResponse, aWebConsoleClient)
  {
    if (aResponse.error) {
      Cu.reportError("attachConsole failed: " + aResponse.error + " " +
                     aResponse.message);
    }

    aState.client = aWebConsoleClient;

    aCallback(aState, aResponse);
  }

  connectToDebugger(function _onConnect(aState, aResponse) {
    if (aResponse.error) {
      Cu.reportError("client.connect() failed: " + aResponse.error + " " +
                     aResponse.message);
      aCallback(aState, aResponse);
      return;
    }

    aState.dbgClient.listTabs(function _onListTabs(aResponse) {
      if (aResponse.error) {
        Cu.reportError("listTabs failed: " + aResponse.error + " " +
                       aResponse.message);
        aCallback(aState, aResponse);
        return;
      }
      let consoleActor = aAttachToTab ?
                         aResponse.tabs[aResponse.selected].consoleActor :
                         aResponse.consoleActor;
      aState.actor = consoleActor;
      aState.dbgClient.attachConsole(consoleActor, aListeners,
                                     _onAttachConsole.bind(null, aState));
    });
  });
}

function closeDebugger(aState, aCallback)
{
  aState.dbgClient.close(aCallback);
  aState.dbgClient = null;
  aState.client = null;
}

function checkConsoleAPICall(aCall, aExpected)
{
  if (aExpected.level != "trace" && aExpected.arguments) {
    is(aCall.arguments.length, aExpected.arguments.length,
       "number of arguments");
  }

  checkObject(aCall, aExpected);
}

function checkObject(aObject, aExpected)
{
  for (let name of Object.keys(aExpected))
  {
    let expected = aExpected[name];
    let value = aObject[name];
    checkValue(name, value, expected);
  }
}

function checkValue(aName, aValue, aExpected)
{
  if (aExpected === null) {
    ok(!aValue, "'" + aName + "' is null");
  }
  else if (aValue === undefined) {
    ok(false, "'" + aName + "' is undefined");
  }
  else if (typeof aExpected == "string" || typeof aExpected == "number" ||
           typeof aExpected == "boolean") {
    is(aValue, aExpected, "property '" + aName + "'");
  }
  else if (aExpected instanceof RegExp) {
    ok(aExpected.test(aValue), aName + ": " + aExpected + " matched " + aValue);
  }
  else if (Array.isArray(aExpected)) {
    info("checking array for property '" + aName + "'");
    checkObject(aValue, aExpected);
  }
  else if (typeof aExpected == "object") {
    info("checking object for property '" + aName + "'");
    checkObject(aValue, aExpected);
  }
}

function checkHeadersOrCookies(aArray, aExpected)
{
  let foundHeaders = {};

  for (let elem of aArray) {
    if (!(elem.name in aExpected)) {
      continue;
    }
    foundHeaders[elem.name] = true;
    info("checking value of header " + elem.name);
    checkValue(elem.name, elem.value, aExpected[elem.name]);
  }

  for (let header in aExpected) {
    if (!(header in foundHeaders)) {
      ok(false, header + " was not found");
    }
  }
}

var gTestState = {};

function runTests(aTests, aEndCallback)
{
  function driver()
  {
    let lastResult, sendToNext;
    for (let i = 0; i < aTests.length; i++) {
      gTestState.index = i;
      let fn = aTests[i];
      info("will run test #" + i + ": " + fn.name);
      lastResult = fn(sendToNext, lastResult);
      sendToNext = yield lastResult;
    }
    yield aEndCallback(sendToNext, lastResult);
  }
  gTestState.driver = driver();
  return gTestState.driver.next();
}

function nextTest(aMessage)
{
  return gTestState.driver.send(aMessage);
}
