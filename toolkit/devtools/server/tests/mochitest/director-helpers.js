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

const {Class} = devtools.require("sdk/core/heritage");

const {promiseInvoke} = devtools.require("devtools/async-utils");

const { DirectorRegistry,
        DirectorRegistryFront } = devtools.require("devtools/server/actors/director-registry");

const { DirectorManagerFront } = devtools.require("devtools/server/actors/director-manager");
const protocol = devtools.require("devtools/server/protocol");

const {Task} = devtools.require("resource://gre/modules/Task.jsm");

/***********************************
 *  director helpers functions
 **********************************/

function waitForEvent(target, name) {
  return new Promise((resolve, reject) => {
      target.once(name, (...args) => { resolve(args); });
  });
}

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

function* installTestDirectorScript(client, root,  scriptId, scriptDefinition) {
  var directorRegistryClient = new DirectorRegistryFront(client, root);

  yield directorRegistryClient.install(scriptId, scriptDefinition);

  directorRegistryClient.destroy();
}

function* getTestDirectorScript(manager, tab, scriptId) {
  var directorScriptClient = yield manager.getByScriptId(scriptId);
  return directorScriptClient;
}

function purgeInstalledDirectorScripts() {
  DirectorRegistry.clear();
}

function* installDirectorScriptAndWaitAttachOrError({client, root, manager,
                                                     scriptId, scriptDefinition}) {
  yield installTestDirectorScript(client, root, scriptId, scriptDefinition);

  var selectedTab = root.tabs[root.selected];
  var testDirectorScriptClient = yield getTestDirectorScript(manager, selectedTab, scriptId);

  var waitForDirectorScriptAttach = waitForEvent(testDirectorScriptClient, "attach");
  var waitForDirectorScriptError = waitForEvent(testDirectorScriptClient, "error");

  testDirectorScriptClient.setup({reload: false});

  var [receivedEvent] = yield Promise.race([waitForDirectorScriptAttach,
                                            waitForDirectorScriptError]);

  testDirectorScriptClient.finalize();

  return receivedEvent;
}

function assertIsDirectorScriptError(error) {
  ok(!!error, "received error should be defined");
  ok(!!error.message, "errors should contain a message");
  ok(!!error.stack, "errors should contain a stack trace");
  ok(!!error.fileName, "errors should contain a fileName");
  ok(typeof error.columnNumber == "number", "errors should contain a columnNumber");
  ok(typeof error.lineNumber == "number", "errors should contain a lineNumber");

  ok(!!error.directorScriptId, "errors should contain a directorScriptId");
}
