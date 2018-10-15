"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "aomStartup",
                                   "@mozilla.org/addons/addon-manager-startup;1",
                                   "amIAddonManagerStartup");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

/* globals ExtensionAPI */

const PREFIX = "tart@mozilla.org";

this.tart = class extends ExtensionAPI {
  constructor(...args) {
    super(...args);
    this.loadedWindows = new WeakSet();
  }

  onStartup() {
    const manifestURI = Services.io.newURI("manifest.json", null, this.extension.rootURI);
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "tart", "chrome/"],
    ]);

    this.framescriptURL = this.extension.baseURI.resolve("/content/framescript.js");
    Services.mm.loadFrameScript(this.framescriptURL, true);
    Services.mm.addMessageListener(`${PREFIX}:chrome-exec-message`, this);
  }

  onShutdown() {
    Services.mm.removeMessageListener(`${PREFIX}:chrome-exec-message`, this);
    Services.mm.removeDelayedFrameScript(this.framescriptURL);
    this.chromeHandle.destruct();
  }

  receiveMessage({target, data}) {
    let win = target.ownerGlobal;
    if (!this.loadedWindows.has(win)) {
      let {baseURI} = this.extension;
      Services.scriptloader.loadSubScript(baseURI.resolve("/content/Profiler.js"), win);
      Services.scriptloader.loadSubScript(baseURI.resolve("/content/tart.js"), win);
      this.loadedWindows.add(win);
    }

    function sendResult(result) {
      target.messageManager.sendAsyncMessage(`${PREFIX}:chrome-exec-reply`,
                                             {id: data.id, result});
    }

    let {command} = data;

    switch (command.name) {
      case "ping":
        sendResult();
        break;

      case "runTest":
        (new win.Tart()).startTest(sendResult, command.data);
        break;

      case "setASAP":
        Services.prefs.setIntPref("layout.frame_rate", 0);
        Services.prefs.setIntPref("docshell.event_starvation_delay_hint", 1);
        sendResult();
        break;

      case "unsetASAP":
        Services.prefs.clearUserPref("layout.frame_rate");
        Services.prefs.clearUserPref("docshell.event_starvation_delay_hint");
        sendResult();
        break;

      case "toClipboard":
        clipboardHelper.copyString(command.data);
        sendResult();
        break;

      default:
        Cu.reportError(`Unknown TART command ${command.name}\n`);
        break;
    }
  }
};
