var Cu = Components.utils;
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");

const Services = devtools.require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
Services.prefs.setBoolPref("dom.mozBrowserFramesEnabled", true);

SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
  Services.prefs.clearUserPref("dom.mozBrowserFramesEnabled");
});

const {promiseInvoke} = devtools.require("devtools/async-utils");

const { DirectorRegistry,
        DirectorRegistryFront } = devtools.require("devtools/server/actors/director-registry");

const { DirectorManagerFront } = devtools.require("devtools/server/actors/director-manager");

const {Task} = devtools.require("resource://gre/modules/Task.jsm");

/***********************************
 *  director helpers functions
 **********************************/

function* newConnectedDebuggerClient(opts) {
  var transport = DebuggerServer.connectPipe();
  var client = new DebuggerClient(transport);

  yield promiseInvoke(client, client.connect);

  var root = yield promiseInvoke(client, client.listTabs);

  return {
    client: client,
    root: root,
    transport: transport
  };
}

function purgeInstalledDirectorScripts() {
  DirectorRegistry.clear();
}
