Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

let Scope = {}
Components.utils.import("resource://gre/modules/CrashMonitor.jsm", Scope);
let MonitorAPI = Scope.CrashMonitor;

function CrashMonitor() {};

CrashMonitor.prototype = {

  classID: Components.ID("{d9d75e86-8f17-4c57-993e-f738f0d86d42}"),
  contractID: "@mozilla.org/toolkit/crashmonitor;1",

  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIObserver]),

  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
    case "profile-after-change":
      MonitorAPI.init();
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashMonitor]);
