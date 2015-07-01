Cu.import("resource://gre/modules/Services.jsm");
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
let { RemoteDebugger } = Cu.import("resource://webapprt/modules/RemoteDebugger.jsm", {});

function test() {
  waitForExplicitFinish();

  loadWebapp("debugger.webapp", undefined, () => {
    RemoteDebugger.init(Services.prefs.getIntPref('devtools.debugger.remote-port'));

    let client = new DebuggerClient(DebuggerServer.connectPipe());
    client.connect(() => {
      client.listTabs((aResponse) => {
        is(aResponse.tabs[0].title, "Debugger Test Webapp", "Title correct");
        is(aResponse.tabs[0].url, "http://test/webapprtChrome/webapprt/test/chrome/debugger.html", "URL correct");
        ok(aResponse.tabs[0].consoleActor, "consoleActor set");
        ok(aResponse.tabs[0].gcliActor, "gcliActor set");
        ok(aResponse.tabs[0].styleEditorActor, "styleEditorActor set");
        ok(aResponse.tabs[0].inspectorActor, "inspectorActor set");
        ok(aResponse.tabs[0].traceActor, "traceActor set");
        ok(aResponse.deviceActor, "deviceActor set");

        client.close(() => {
          finish();
        });
      });
    });
  });

  registerCleanupFunction(function() {
    DebuggerServer.destroy();
  });
}
