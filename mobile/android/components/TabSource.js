const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Prompt.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "sendMessageToJava",
                                  "resource://gre/modules/Messaging.jsm");

function TabSource() {
}

TabSource.prototype = {
  classID: Components.ID("{5850c76e-b916-4218-b99a-31f004e0a7e7}"),
  classDescription: "Fennec Tab Source",
  contractID: "@mozilla.org/tab-source-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITabSource]),

  getTabToStream: function() {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    if (tabs == null || tabs.length == 0) {
      Services.console.logStringMessage("ERROR: No tabs");
      return null;
    }

    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let title = bundle.GetStringFromName("tabshare.title")

    let prompt = new Prompt({
      title: title,
      window: null
    }).setSingleChoiceItems(tabs.map(function(tab) {
      return { label: tab.browser.contentTitle || tab.browser.contentURI.spec }
    }));

    let result = null;
    prompt.show(function(data) {
      result = data.button;
    });

    // Spin this thread while we wait for a result.
    let thread = Services.tm.currentThread;
    while (result == null) {
      thread.processNextEvent(true);
    }

    if (result == -1) {
      return null;
    }
    return tabs[result].browser.contentWindow;
  },

  notifyStreamStart: function(window) {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    for (var i in tabs) {
      if (tabs[i].browser.contentWindow == window) {
        sendMessageToJava({ type: "Tab:Streaming", tabID: tabs[i].id });
      }
    }
  },

  notifyStreamStop: function(window) {
    let app = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
    let tabs = app.tabs;
    for (let i in tabs) {
      if (tabs[i].browser.contentWindow == window) {
        sendMessageToJava({ type: "Tab:NotStreaming", tabID: tabs[i].id });
      }
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TabSource]);
