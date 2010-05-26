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

  for (let keyval in Iterator(baseForm)) {
    // Make a local copy of these values
    let [key, val] = keyval;

    // Don't overwrite something we already have
    if (key in this)
      continue;

    // Make a getter to grab non-functions
    if (typeof val != "function") {
      this.__defineGetter__(key, function() baseForm[key]);
      continue;
    }

    // Wrap the function with notifications
    this[key] = function() {
      let args = Array.slice(arguments);
      let notify = function(type) {
        obs.notifyObservers(null, "form-notifier", JSON.stringify({
          args: args,
          func: key,
          type: type
        }));
      };

      notify("before");
      try {
        return val.apply(this, arguments);
      }
      finally {
        notify("after");
      }
    };
  }
}
FormNotifier.prototype = {
  classDescription: "Form Notifier Wrapper",
  contractID: "@mozilla.org/satchel/form-history;1",
  classID: Components.ID("{be5a097b-6ee6-4c6a-8eca-6bce87d570e9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormHistory2]),
};

let components = [FormNotifier];
function NSGetModule(compMgr, fileSpec) XPCOMUtils.generateModule(components);
