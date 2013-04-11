/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

function FormHistoryStartup() { }

FormHistoryStartup.prototype = {
  classID: Components.ID("{3A0012EB-007F-4BB8-AA81-A07385F77A25}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
    Ci.nsIFrameMessageListener
  ]),

  observe: function(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        FormHistory.updatePrefs();
        break;
      case "idle-daily":
      case "formhistory-expire-now":
        FormHistory.expireOldEntries();
        break;
      case "profile-before-change":
        FormHistory.shutdown();
        break;
      case "profile-after-change":
        this.init();
      default:
        break;
    }
  },

  inited: false,

  init: function()
  {
    if (this.inited)
      return;
    this.inited = true;

    Services.prefs.addObserver("browser.formfill.", this, true);

    // triggers needed service cleanup and db shutdown
    Services.obs.addObserver(this, "profile-before-change", true);
    Services.obs.addObserver(this, "formhistory-expire-now", true);

    let messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                         getService(Ci.nsIMessageListenerManager);
    messageManager.loadFrameScript("chrome://satchel/content/formSubmitListener.js", true);
    messageManager.addMessageListener("FormHistory:FormSubmitEntries", this);
  },

  receiveMessage: function(message) {
    let entries = message.json;
    let changes = entries.map(function(entry) {
      return {
        op : "bump",
        fieldname : entry.name,
        value : entry.value,
      }
    });

    FormHistory.update(changes);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormHistoryStartup]);
