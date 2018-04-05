/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["EventDispatcher"];

XPCOMUtils.defineLazyServiceGetter(this, "UUIDGen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const IS_PARENT_PROCESS = (Services.appinfo.processType ==
                           Services.appinfo.PROCESS_TYPE_DEFAULT);

function DispatcherDelegate(aDispatcher, aMessageManager) {
  this._dispatcher = aDispatcher;
  this._messageManager = aMessageManager;
}

DispatcherDelegate.prototype = {
  /**
   * Register a listener to be notified of event(s).
   *
   * @param aListener Target listener implementing nsIAndroidEventListener.
   * @param aEvents   String or array of strings of events to listen to.
   */
  registerListener: function(aListener, aEvents) {
    if (!this._dispatcher) {
      throw new Error("Can only listen in parent process");
    }
    this._dispatcher.registerListener(aListener, aEvents);
  },

  /**
   * Unregister a previously-registered listener.
   *
   * @param aListener Registered listener implementing nsIAndroidEventListener.
   * @param aEvents   String or array of strings of events to stop listening to.
   */
  unregisterListener: function(aListener, aEvents) {
    if (!this._dispatcher) {
      throw new Error("Can only listen in parent process");
    }
    this._dispatcher.unregisterListener(aListener, aEvents);
  },

  /**
   * Dispatch an event to registered listeners for that event, and pass an
   * optional data object and/or a optional callback interface to the
   * listeners.
   *
   * @param aEvent     Name of event to dispatch.
   * @param aData      Optional object containing data for the event.
   * @param aCallback  Optional callback implementing nsIAndroidEventCallback.
   * @param aFinalizer Optional finalizer implementing nsIAndroidEventFinalizer.
   */
  dispatch: function(aEvent, aData, aCallback, aFinalizer) {
    if (this._dispatcher) {
      this._dispatcher.dispatch(aEvent, aData, aCallback, aFinalizer);
      return;
    }

    let mm = this._messageManager || Services.cpmm;
    let forwardData = {
      global: !this._messageManager,
      event: aEvent,
      data: aData,
    };

    if (aCallback) {
      forwardData.uuid = UUIDGen.generateUUID().toString();
      mm.addMessageListener("GeckoView:MessagingReply", function listener(msg) {
        if (msg.data.uuid !== forwardData.uuid) {
          return;
        }
        if (msg.data.type === "success") {
          aCallback.onSuccess(msg.data.response);
        } else if (msg.data.type === "error") {
          aCallback.onError(msg.data.response);
        } else if (msg.data.type === "finalize") {
          aFinalizer && aFinalizer.onFinalize();
          mm.removeMessageListener(msg.name, listener);
        } else {
          throw new Error("invalid reply type");
        }
      });
    }

    mm.sendAsyncMessage("GeckoView:Messaging", forwardData);
  },

  /**
   * Sends a request to Java.
   *
   * @param aMsg      Message to send; must be an object with a "type" property
   * @param aCallback Optional callback implementing nsIAndroidEventCallback.
   */
  sendRequest: function(aMsg, aCallback) {
    const type = aMsg.type;
    aMsg.type = undefined;
    this.dispatch(type, aMsg, aCallback);
  },

  /**
   * Sends a request to Java, returning a Promise that resolves to the response.
   *
   * @param aMsg Message to send; must be an object with a "type" property
   * @return A Promise resolving to the response
   */
  sendRequestForResult: function(aMsg) {
    return new Promise((resolve, reject) => {
      const type = aMsg.type;
      aMsg.type = undefined;

      this.dispatch(type, aMsg, {
        onSuccess: resolve,
        onError: reject,
      });
    });
  },
};

var EventDispatcher = {
  instance: new DispatcherDelegate(IS_PARENT_PROCESS ? Services.androidBridge : undefined),

  /**
   * Return an EventDispatcher instance for a chrome DOM window. In a content
   * process, return a proxy through the message manager that automatically
   * forwards events to the main process.
   *
   * To force using a message manager proxy (for example in a frame script
   * environment), call forMessageManager.
   *
   * @param aWindow a chrome DOM window.
   */
  for: function(aWindow) {
    let view = aWindow && aWindow.arguments && aWindow.arguments[0] &&
               aWindow.arguments[0].QueryInterface(Ci.nsIAndroidView);

    if (!view) {
      let mm = !IS_PARENT_PROCESS && aWindow && aWindow.messageManager;
      if (!mm) {
        throw new Error("window is not a GeckoView-connected window and does" +
                        " not have a message manager");
      }
      return this.forMessageManager(mm);
    }

    return new DispatcherDelegate(view);
  },

  /**
   * Return an EventDispatcher instance for a message manager associated with a
   * window.
   *
   * @param aWindow a message manager.
   */
  forMessageManager: function(aMessageManager) {
    return new DispatcherDelegate(null, aMessageManager);
  },

  receiveMessage: function(aMsg) {
    // aMsg.data includes keys: global, event, data, uuid
    let callback;
    if (aMsg.data.uuid) {
      let reply = (type, response) => {
        let mm = aMsg.data.global ? aMsg.target : aMsg.target.messageManager;
        mm.sendAsyncMessage("GeckoView:MessagingReply", {
          type: type,
          response: response,
          uuid: aMsg.data.uuid,
        });
      };
      callback = {
        onSuccess: response => reply("success", response),
        onError: error => reply("error", error),
        onFinalize: () => reply("finalize"),
      };
    }

    if (aMsg.data.global) {
      this.instance.dispatch(aMsg.data.event, aMsg.data.data,
                             callback, callback);
      return;
    }

    let win = aMsg.target.ownerGlobal;
    let dispatcher = win.WindowEventDispatcher || this.for(win);
    dispatcher.dispatch(aMsg.data.event, aMsg.data.data, callback, callback);
  },
};

if (IS_PARENT_PROCESS) {
  Services.mm.addMessageListener("GeckoView:Messaging", EventDispatcher);
  Services.ppmm.addMessageListener("GeckoView:Messaging", EventDispatcher);
}
