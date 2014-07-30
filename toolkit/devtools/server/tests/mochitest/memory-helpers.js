let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
let { require } = devtools;

let { MemoryFront } = require("devtools/server/actors/memory");

function startServerAndGetSelectedTabMemory() {
  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();
  var client = new DebuggerClient(DebuggerServer.connectPipe());

  return new Promise((resolve, reject) => {
    client.connect(response => {
      if (response.error) {
        reject(new Error(response.error + ": " + response.message));
        return;
      }

      client.listTabs(response => {
        if (response.error) {
          reject(new Error(response.error + ": " + response.message));
          return;
        }

        var form = response.tabs[response.selected];
        var memory = MemoryFront(client, form);

        resolve({ memory, client });
      });
    });
  });
}

function destroyServerAndFinish(client) {
  client.close(() => {
    DebuggerServer.destroy();
    SimpleTest.finish()
  });
}
