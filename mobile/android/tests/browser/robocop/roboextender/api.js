
ChromeUtils.import("resource://gre/modules/Messaging.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

this.roboextender = class extends ExtensionAPI {
  onStartup() {
    let aomStartup = Cc["@mozilla.org/addons/addon-manager-startup;1"]
                                 .getService(Ci.amIAddonManagerStartup);
    const manifestURI = Services.io.newURI("manifest.json", null, this.extension.rootURI);
    const targetURL = this.extension.rootURI.resolve("base/");
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "roboextender", targetURL],
    ]);

    EventDispatcher.instance.registerListener(function(event, data, callback) {
      dump("Robocop:Quit received -- requesting quit");
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
    }, "Robocop:Quit");
  }

  onShutdown() {
    this.chromeHandle.destruct();
    this.chromeHandle = null;
  }
};
