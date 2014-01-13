/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
const { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm");
const { Services } = Cu.import("resource://gre/modules/Services.jsm");

let gClient, gActor;

function connect(onDone) {

  if (Services.appinfo.name == "B2G") {
    // On b2g, we try to exercice the code that launches the production debugger server
    let settingsService = Cc["@mozilla.org/settingsService;1"].getService(Ci.nsISettingsService);
    settingsService.createLock().set("devtools.debugger.remote-enabled", true, null);
    // We can't use `set` callback as it is fired before shell.js code listening for this setting
    // is actually called. Same thing applies to mozsettings-changed obs notification.
    // So listen to a custom event until bug 942756 lands
    let observer = {
      observe: function (subject, topic, data) {
        Services.obs.removeObserver(observer, "debugger-server-started");
        let transport = debuggerSocketConnect("127.0.0.1", 6000);
        startClient(transport, onDone);
      }
    };
    Services.obs.addObserver(observer, "debugger-server-started", false);
  } else {
    // Initialize a loopback remote protocol connection
    DebuggerServer.init(function () { return true; });
    // We need to register browser actors to have `listTabs` working
    // and also have a root actor
    DebuggerServer.addBrowserActors();
    let transport = DebuggerServer.connectPipe();
    startClient(transport, onDone);
  }
}

function startClient(transport, onDone) {
  // Setup client and actor used in all tests
  gClient = new DebuggerClient(transport);
  gClient.connect(function onConnect() {
    gClient.listTabs(function onListTabs(aResponse) {
      gActor = aResponse.webappsActor;
      if (gActor)
        webappActorRequest({type: "watchApps"}, onDone);
    });
  });

  gClient.addListener("appInstall", function (aState, aType, aPacket) {
    sendAsyncMessage("installed-event", { manifestURL: aType.manifestURL });
  });

  gClient.addListener("appUninstall", function (aState, aType, aPacket) {
    sendAsyncMessage("uninstalled-event", { manifestURL: aType.manifestURL });
  });
}

function webappActorRequest(request, onResponse) {
  if (!gActor) {
    connect(webappActorRequest.bind(null, request, onResponse));
    return;
  }

  request.to = gActor;
  gClient.request(request, onResponse);
}


function downloadURL(url, file) {
  let channel = Services.io.newChannel(url, null, null);
  let istream = channel.open();
  let bstream = Cc["@mozilla.org/binaryinputstream;1"]
                  .createInstance(Ci.nsIBinaryInputStream);
  bstream.setInputStream(istream);
  let data = bstream.readBytes(bstream.available());

  let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                  .createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, 0x04 | 0x08 | 0x20, 0600, 0);
  ostream.write(data, data.length);
  ostream.QueryInterface(Ci.nsISafeOutputStream).finish();
}

// Install a test packaged webapp from data folder
addMessageListener("install", function (aMessage) {
  let url = aMessage.url;
  let appId = aMessage.appId;

  try {
    // Download its content from mochitest http server
    // Copy our package to tmp folder, where the actor retrieves it
    let zip = FileUtils.getDir("TmpD", ["b2g", appId], true, true);
    zip.append("application.zip");
    downloadURL(url, zip);

    let request = {type: "install", appId: appId};
    webappActorRequest(request, function (aResponse) {
      sendAsyncMessage("installed", aResponse);
    });
  } catch(e) {
    dump("installTestApp exception: " + e + "\n");
  }
});

addMessageListener("cleanup", function () {
  webappActorRequest({type: "unwatchApps"}, function () {
    gClient.close();
  });
});

