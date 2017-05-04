/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "ContentPrefServiceParent" ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "_methodsCallableFromChild",
                                  "resource://gre/modules/ContentPrefUtils.jsm");

let loadContext = Cc["@mozilla.org/loadcontext;1"].
                    createInstance(Ci.nsILoadContext);
let privateLoadContext = Cc["@mozilla.org/privateloadcontext;1"].
                           createInstance(Ci.nsILoadContext);

function contextArg(context) {
  return (context && context.usePrivateBrowsing) ?
           privateLoadContext :
           loadContext;
}

var ContentPrefServiceParent = {
  // Called on all platforms.
  alwaysInit() {
    let globalMM = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);

    globalMM.addMessageListener("child-process-shutdown", this);
  },

  // Only called on Android. Listeners are added in nsBrowserGlue.js on other
  // platforms.
  init() {
    let globalMM = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);

    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN nsBrowserGlue
    globalMM.addMessageListener("ContentPrefs:FunctionCall", this);
    globalMM.addMessageListener("ContentPrefs:AddObserverForName", this);
    globalMM.addMessageListener("ContentPrefs:RemoveObserverForName", this);
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN nsBrowserGlue

    this.alwaysInit();
  },

  // Map from message manager -> content pref observer.
  _observers: new Map(),

  handleObserverChange(msg) {
    let observer = this._observers.get(msg.target);
    if (msg.name === "child-process-shutdown") {
      // If we didn't have any observers for this child process, don't do
      // anything.
      if (!observer)
        return;

      for (let i of observer._names) {
        this._cps2.removeObserverForName(i, observer);
      }

      this._observers.delete(msg.target);
      return;
    }

    let prefName = msg.data.name;
    if (msg.name === "ContentPrefs:AddObserverForName") {
      // The child process is responsible for not adding multiple parent
      // observers for the same name.
      if (!observer) {
        observer = {
          onContentPrefSet(group, name, value, isPrivate) {
            msg.target.sendAsyncMessage("ContentPrefs:NotifyObservers",
                                        { name, callback: "onContentPrefSet",
                                          args: [ group, name, value, isPrivate ] });
          },

          onContentPrefRemoved(group, name, isPrivate) {
            msg.target.sendAsyncMessage("ContentPrefs:NotifyObservers",
                                        { name, callback: "onContentPrefRemoved",
                                          args: [ group, name, isPrivate ] });
          },

          // The names we're using this observer object for, used to keep track
          // of the number of names we care about as well as for removing this
          // observer if its associated process goes away.
          _names: new Set()
        };

        this._observers.set(msg.target, observer);
      }

      observer._names.add(prefName);

      this._cps2.addObserverForName(prefName, observer);
    } else {
      // RemoveObserverForName

      // We must have an observer.
      this._cps2.removeObserverForName(prefName, observer);

      observer._names.delete(prefName);
      if (observer._names.size === 0) {
        // This was the last use for this observer.
        this._observers.delete(msg.target);
      }
    }
  },

  // Listeners are added in nsBrowserGlue.js
  receiveMessage(msg) {
    if (msg.name != "ContentPrefs:FunctionCall") {
      this.handleObserverChange(msg);
      return;
    }

    let data = msg.data;
    let signature;

    if (!_methodsCallableFromChild.some(([method, args]) => {
       if (method == data.call) {
         signature = args;
         return true;
       }
       return false;
     })) {
      throw new Error(`Can't call ${data.call} from child!`);
    }

    let args = data.args;
    let requestId = data.requestId;

    let listener = {
      handleResult(pref) {
        msg.target.sendAsyncMessage("ContentPrefs:HandleResult",
                                    { requestId,
                                      contentPref: {
                                        domain: pref.domain,
                                        name: pref.name,
                                        value: pref.value
                                      }
                                    });
      },

      handleError(error) {
        msg.target.sendAsyncMessage("ContentPrefs:HandleError",
                                    { requestId,
                                      error });
      },
      handleCompletion(reason) {
        msg.target.sendAsyncMessage("ContentPrefs:HandleCompletion",
                                    { requestId,
                                      reason });
      }
    };

    // Push our special listener.
    args.push(listener);

    // Process context argument for forwarding
    let contextIndex = signature.indexOf("context");
    if (contextIndex > -1) {
      args[contextIndex] = contextArg(args[contextIndex]);
    }

    // And call the function.
    this._cps2[data.call](...args);
  }
};

XPCOMUtils.defineLazyServiceGetter(ContentPrefServiceParent, "_cps2",
                                   "@mozilla.org/content-pref/service;1",
                                   "nsIContentPrefService2");
