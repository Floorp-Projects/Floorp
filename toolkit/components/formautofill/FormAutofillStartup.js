/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Handles startup in the parent process.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofill",
                                  "resource://gre/modules/FormAutofill.jsm");

/**
 * Handles startup in the parent process.
 */
function FormAutofillStartup() {
}

FormAutofillStartup.prototype = {
  classID: Components.ID("{51c95b3d-7431-467b-8d50-383f158ce9e5}"),
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIFrameMessageListener,
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  // nsIObserver
  observe: function (aSubject, aTopic, aData) {
    // This method is called by the "profile-after-change" category on startup,
    // which is called before any web page loads.  At this time, we need to
    // register a global message listener in the parent process preemptively,
    // because we can receive requests from child processes at any time.  For
    // performance reasons, we use this object as a message listener, so that we
    // don't have to load the FormAutoFill module at startup.
    let globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);
    globalMM.addMessageListener("FormAutofill:RequestAutocomplete", this);
  },

  // nsIFrameMessageListener
  receiveMessage: function (aMessage) {
    // Process the "FormAutofill:RequestAutocomplete" message.  Any exception
    // raised in the parent process is caught and serialized into the reply
    // message that is sent to the requesting child process.
    FormAutofill.processRequestAutocomplete(aMessage.data)
      .catch(ex => { return { exception: ex } })
      .then(result => {
        // The browser message manager in the parent will send the reply to the
        // associated frame message manager in the child.
        let browserMM = aMessage.target.messageManager;
        browserMM.sendAsyncMessage("FormAutofill:RequestAutocompleteResult",
                                   result);
      })
      .catch(Cu.reportError);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FormAutofillStartup]);
