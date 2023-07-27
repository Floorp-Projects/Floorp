/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * RemotePageChild is a base class for an unprivileged internal page, typically
 * an about: page. A specific implementation should subclass the RemotePageChild
 * actor with a more specific actor for that page. Typically, the child is not
 * needed, but the parent actor will respond to messages and provide results
 * directly to the page.
 */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncPrefs: "resource://gre/modules/AsyncPrefs.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemotePageAccessManager:
    "resource://gre/modules/RemotePageAccessManager.sys.mjs",
});

export class RemotePageChild extends JSWindowActorChild {
  actorCreated() {
    this.listeners = new Map();
    this.exportBaseFunctions();
  }

  exportBaseFunctions() {
    const exportableFunctions = [
      "RPMSendAsyncMessage",
      "RPMSendQuery",
      "RPMAddMessageListener",
      "RPMRemoveMessageListener",
      "RPMGetIntPref",
      "RPMGetStringPref",
      "RPMGetBoolPref",
      "RPMSetPref",
      "RPMGetFormatURLPref",
      "RPMIsWindowPrivate",
    ];

    this.exportFunctions(exportableFunctions);
  }

  /**
   * Exports a list of functions to be accessible by the privileged page.
   * Subclasses may call this function to add functions that are specific
   * to a page. When the page calls a function, a function with the same
   * name is called within the child actor.
   *
   * Only functions that appear in the whitelist in the
   * RemotePageAccessManager for that page will be exported.
   *
   * @param array of function names.
   */
  exportFunctions(functions) {
    let document = this.document;
    let principal = document.nodePrincipal;

    // If there is no content principal, don't export any functions.
    if (!principal) {
      return;
    }

    let window = this.contentWindow;

    for (let fnname of functions) {
      let allowAccess = lazy.RemotePageAccessManager.checkAllowAccessToFeature(
        principal,
        fnname,
        document
      );

      if (allowAccess) {
        // Wrap each function in an access checking function.
        function accessCheckedFn(...args) {
          this.checkAllowAccess(fnname, args[0]);
          return this[fnname](...args);
        }

        Cu.exportFunction(accessCheckedFn.bind(this), window, {
          defineAs: fnname,
        });
      }
    }
  }

  handleEvent() {
    // Do nothing. The DOMDocElementInserted event is just used to create
    // the actor.
  }

  receiveMessage(messagedata) {
    let message = {
      name: messagedata.name,
      data: messagedata.data,
    };

    let listeners = this.listeners.get(message.name);
    if (!listeners) {
      return;
    }

    let clonedMessage = Cu.cloneInto(message, this.contentWindow);
    for (let listener of listeners.values()) {
      try {
        listener(clonedMessage);
      } catch (e) {
        console.error(e);
      }
    }
  }

  wrapPromise(promise) {
    return new this.contentWindow.Promise((resolve, reject) =>
      promise.then(resolve, reject)
    );
  }

  /**
   * Returns true if a feature cannot be accessed by the current page.
   * Throws an exception if the feature may not be accessed.

   * @param aDocument child process document to call from
   * @param aFeature to feature to check access to
   * @param aValue value that must be included with that feature's whitelist
   * @returns true if access is allowed or throws an exception otherwise
   */
  checkAllowAccess(aFeature, aValue) {
    let doc = this.document;
    if (!lazy.RemotePageAccessManager.checkAllowAccess(doc, aFeature, aValue)) {
      throw new Error(
        "RemotePageAccessManager does not allow access to " + aFeature
      );
    }

    return true;
  }

  addPage(aUrl, aFunctionMap) {
    lazy.RemotePageAccessManager.addPage(aUrl, aFunctionMap);
  }

  // Implementation of functions that are exported into the page.

  RPMSendAsyncMessage(aName, aData = null) {
    this.sendAsyncMessage(aName, aData);
  }

  RPMSendQuery(aName, aData = null) {
    return this.wrapPromise(
      new Promise(resolve => {
        this.sendQuery(aName, aData).then(result => {
          resolve(Cu.cloneInto(result, this.contentWindow));
        });
      })
    );
  }

  /**
   * Adds a listener for messages. Many callbacks can be registered for the
   * same message if necessary. An attempt to register the same callback for the
   * same message twice will be ignored. When called the callback is passed an
   * object with these properties:
   *   name:   The message name
   *   data:   Any data sent with the message
   */
  RPMAddMessageListener(aName, aCallback) {
    if (!this.listeners.has(aName)) {
      this.listeners.set(aName, new Set([aCallback]));
    } else {
      this.listeners.get(aName).add(aCallback);
    }
  }

  /**
   * Removes a listener for messages.
   */
  RPMRemoveMessageListener(aName, aCallback) {
    if (!this.listeners.has(aName)) {
      return;
    }

    this.listeners.get(aName).delete(aCallback);
  }

  RPMGetIntPref(aPref, defaultValue) {
    // Only call with a default value if it's defined, to be able to throw
    // errors for non-existent prefs.
    if (defaultValue !== undefined) {
      return Services.prefs.getIntPref(aPref, defaultValue);
    }
    return Services.prefs.getIntPref(aPref);
  }

  RPMGetStringPref(aPref) {
    return Services.prefs.getStringPref(aPref);
  }

  RPMGetBoolPref(aPref, defaultValue) {
    // Only call with a default value if it's defined, to be able to throw
    // errors for non-existent prefs.
    if (defaultValue !== undefined) {
      return Services.prefs.getBoolPref(aPref, defaultValue);
    }
    return Services.prefs.getBoolPref(aPref);
  }

  RPMSetPref(aPref, aVal) {
    return this.wrapPromise(lazy.AsyncPrefs.set(aPref, aVal));
  }

  RPMGetFormatURLPref(aFormatURL) {
    return Services.urlFormatter.formatURLPref(aFormatURL);
  }

  RPMIsWindowPrivate() {
    return lazy.PrivateBrowsingUtils.isContentWindowPrivate(this.contentWindow);
  }
}
