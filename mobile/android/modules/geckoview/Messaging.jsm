/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["sendMessageToJava", "Messaging", "EventDispatcher"];

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

function sendMessageToJava(aMessage, aCallback) {
  Cu.reportError("sendMessageToJava is deprecated. Use EventDispatcher instead.");

  if (aCallback) {
    EventDispatcher.instance.sendRequestForResult(aMessage)
      .then(result => aCallback(result, null),
            error => aCallback(null, error));
  } else {
    EventDispatcher.instance.sendRequest(aMessage);
  }
}

function DispatcherDelegate(dispatcher) {
  this._dispatcher = dispatcher;
}

DispatcherDelegate.prototype = {
  /**
   * Register a listener to be notified of event(s).
   *
   * @param listener Target listener implementing nsIAndroidEventListener.
   * @param events   String or array of strings of events to listen to.
   */
  registerListener: function (listener, events) {
    this._dispatcher.registerListener(listener, events);
  },

  /**
   * Unregister a previously-registered listener.
   *
   * @param listener Registered listener implementing nsIAndroidEventListener.
   * @param events   String or array of strings of events to stop listening to.
   */
  unregisterListener: function (listener, events) {
    this._dispatcher.unregisterListener(listener, events);
  },

  /**
   * Dispatch an event to registered listeners for that event, and pass an
   * optional data object and/or a optional callback interface to the
   * listeners.
   *
   * @param event    Name of event to dispatch.
   * @param data     Optional object containing data for the event.
   * @param callback Optional callback implementing nsIAndroidEventCallback.
   */
  dispatch: function (event, data, callback) {
    this._dispatcher.dispatch(event, data, callback);
  },

  /**
   * Implementations of Messaging APIs for backwards compatibility.
   */

  /**
   * Sends a request to Java.
   *
   * @param msg Message to send; must be an object with a "type" property
   */
  sendRequest: function (msg) {
    let type = msg.type;
    msg.type = undefined;
    this.dispatch(type, msg);
  },

  /**
   * Sends a request to Java, returning a Promise that resolves to the response.
   *
   * @param msg Message to send; must be an object with a "type" property
   * @returns A Promise resolving to the response
   */
  sendRequestForResult: function (msg) {
    return new Promise((resolve, reject) => {
      let type = msg.type;
      msg.type = undefined;

      this.dispatch(type, msg, {
        onSuccess: response => resolve(response),
        onError: response => reject(response)
      });
    });
  },

  /**
   * Add a listener for the given event.
   *
   * Only one request listener can be registered for a given event.
   *
   * Example usage:
   *   // aData is data sent from Java with the request. The return value is
   *   // used to respond to the request. The return type *must* be an instance
   *   // of Object.
   *   let listener = function (aData) {
   *     if (aData == "foo") {
   *       return { response: "bar" };
   *     }
   *     return {};
   *   };
   *   EventDispatcher.instance.addListener(listener, "Demo:Request");
   *
   * The listener may also be a generator function, useful for performing a
   * task asynchronously. For example:
   *   let listener = function* (aData) {
   *     // Respond with "bar" after 2 seconds.
   *     yield new Promise(resolve => setTimeout(resolve, 2000));
   *     return { response: "bar" };
   *   };
   *   EventDispatcher.instance.addListener(listener, "Demo:Request");
   *
   * @param listener Listener callback taking a single data parameter
   *                 (see example usage above).
   * @param event    Event name that this listener should observe.
   */
  addListener: function (listener, event) {
    if (this._requestHandler.listeners[event]) {
      throw new Error("Error in addListener: A listener already exists for event " + event);
    }
    if (typeof listener !== "function") {
      throw new Error("Error in addListener: Listener must be a function for event " + event);
    }

    this._requestHandler.listeners[event] = listener;
    this.registerListener(this._requestHandler, event);
  },

  /**
   * Removes a listener for a given event.
   *
   * @param event The event to stop listening for.
   */
  removeListener: function (event) {
    if (!this._requestHandler.listeners[event]) {
      throw new Error("Error in removeListener: There is no listener for event " + event);
    }

    this._requestHandler.listeners[event] = undefined;
    this.unregisterListener(this._requestHandler, event);
  },

  _requestHandler: {
    listeners: {},

    onEvent: Task.async(function* (event, data, callback) {
      try {
        let response = yield this.listeners[event](data.data);
        callback.onSuccess(response);

      } catch (e) {
        Cu.reportError("Error in Messaging handler for " + event + ": " + e);

        callback.onError({
          message: e.message || (e && e.toString()),
          stack: e.stack || Components.stack.formattedStack,
        });
      }
    }),
  },
};

var EventDispatcher = {
  instance: new DispatcherDelegate(Services.androidBridge),

  for: function (window) {
    let view = window && window.arguments && window.arguments[0] &&
        window.arguments[0].QueryInterface(Ci.nsIAndroidView);
    if (!view) {
      throw new Error("window is not a GeckoView-connected window");
    }
    return new DispatcherDelegate(view);
  },
};

// For backwards compatibility.
var Messaging = {};

function _addMessagingGetter(name) {
  Messaging[name] = function() {
    Cu.reportError("Messaging." + name + " is deprecated. " +
                   "Use EventDispatcher object instead.");

    // Try global dispatcher first.
    let ret = EventDispatcher.instance[name].apply(EventDispatcher.instance, arguments);
    if (ret) {
      // For sendRequestForResult, return the global dispatcher promise.
      return ret;
    }

    // Now try the window dispatcher.
    let window = Services.wm.getMostRecentWindow("navigator:browser");
    let dispatcher = window && window.WindowEventDispatcher;
    let func = dispatcher && dispatcher[name];
    if (typeof func === "function") {
      return func.apply(dispatcher, arguments);
    }
  };
}

_addMessagingGetter("sendRequest");
_addMessagingGetter("sendRequestForResult");
_addMessagingGetter("addListener");
_addMessagingGetter("removeListener");
