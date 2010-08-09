const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function FormNotifier() {
  let formClass = Components.classesByID["{a2059c0e-5a58-4c55-ab7c-26f0557546ef}"] ||
    Components.classesByID["{0c1bb408-71a2-403f-854a-3a0659829ded}"];
  let baseForm = formClass.getService(Ci.nsIFormHistory2);
  let obs = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);

  function wrap(method) {
    return function() {
      let args = Array.slice(arguments);
      let notify = function(type) {
        obs.notifyObservers(null, "form-notifier", JSON.stringify({
          args: args,
          func: method,
          type: type
        }));
      };

      notify("before");
      try {
        return baseForm[method].apply(this, arguments);
      }
      finally {
        notify("after");
      }
    };
  }

  this.__defineGetter__("DBConnection", function() baseForm.DBConnection);
  this.__defineGetter__("hasEntries", function() baseForm.hasEntries);

  this.addEntry = wrap("addEntry");
  this.entryExists = wrap("entryExists");
  this.nameExists = wrap("nameExists");
  this.removeAllEntries = wrap("removeAllEntries");
  this.removeEntriesByTimeframe = wrap("removeEntriesByTimeframe");
  this.removeEntriesForName = wrap("removeEntriesForName");
  this.removeEntry = wrap("removeEntry");

  // Avoid leaking the base form service.
  obs.addObserver({
    observe: function() {
      obs.removeObserver(this, "profile-before-change");
      baseForm = null;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                           Ci.nsIObserver])
  }, "profile-before-change", true);
}
FormNotifier.prototype = {
  classDescription: "Form Notifier Wrapper",
  contractID: "@mozilla.org/satchel/form-history;1",
  classID: Components.ID("{be5a097b-6ee6-4c6a-8eca-6bce87d570e9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormHistory2]),
};

// Gecko <2.0
function NSGetModule(compMgr, fileSpec) XPCOMUtils.generateModule([FormNotifier]);

// Gecko >=2.0
if (typeof XPCOMUtils.generateNSGetFactory == "function")
    const NSGetFactory = XPCOMUtils.generateNSGetFactory([FormNotifier]);
