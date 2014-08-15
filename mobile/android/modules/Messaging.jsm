/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.EXPORTED_SYMBOLS = ["sendMessageToJava", "RequestService"];

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

function sendMessageToJava(aMessage, aCallback) {
  if (aCallback) {
    let id = uuidgen.generateUUID().toString();
    let obs = {
      observe: function(aSubject, aTopic, aData) {
        let data = JSON.parse(aData);
        if (data.__guid__ != id) {
          return;
        }

        Services.obs.removeObserver(obs, aMessage.type + ":Response", false);

        if (data.status === "cancel") {
          // No Java-side listeners handled our callback.
          return;
        }

        aCallback(data.status === "success" ? data.response : null,
                  data.status === "error"   ? data.response : null);
      }
    }

    aMessage.__guid__ = id;
    Services.obs.addObserver(obs, aMessage.type + ":Response", false);
  }

  return Services.androidBridge.handleGeckoMessage(aMessage);
}

let RequestService = {
  /**
   * Add a listener for the given message.
   *
   * Only one request listener can be registered for a given message.
   *
   * Example usage:
   *   RequestService.addListener({
   *     // aMessage is the message name.
   *     // aData is data sent from Java with the request.
   *     // The return value is used to respond to the request. The return
   *     //   type *must* be an instance of Object.
   *     onRequest: function (aMessage, aData) {
   *       if (aData == "foo") {
   *         return { response: "bar" };
   *       }
   *       return {};
   *     }
   *   }, "Demo:Request");
   *
   * The listener may also be a generator function, useful for performing a
   * task asynchronously. For example:
   *   RequestService.addListener({
   *     onRequest: function* (aMessage, aData) {
   *       yield new Promise(resolve => setTimeout(resolve, 2000));
   *       return { response: "bar" };
   *     }
   *   }, "Demo:Request");
   *
   * @param aListener Listener object with an onRequest function (see example
   *                  usage above).
   * @param aMessage  Event name that this listener should observe.
   */
  addListener: function (aListener, aMessage) {
    requestHandler.addListener(aListener, aMessage);
  },

  /**
   * Removes a listener for a given message.
   *
   * @param aMessage The event to stop listening for.
   */
  removeListener: function (aMessage) {
    requestHandler.removeListener(aMessage);
  },
};

let requestHandler = {
  _listeners: {},

  addListener: function (aListener, aMessage) {
    if (aMessage in this._listeners) {
      throw new Error("Error in addListener: A listener already exists for message " + aMessage);
    }

    if (typeof aListener !== "function") {
      throw new Error("Error in addListener: Listener must be a function for message " + aMessage);
    }

    this._listeners[aMessage] = aListener;
    Services.obs.addObserver(this, aMessage, false);
  },

  removeListener: function (aMessage) {
    if (!(aMessage in this._listeners)) {
      throw new Error("Error in removeListener: There is no listener for message " + aMessage);
    }

    delete this._listeners[aMessage];
    Services.obs.removeObserver(this, aMessage);
  },

  observe: Task.async(function* (aSubject, aTopic, aData) {
    let wrapper = JSON.parse(aData);
    let listener = this._listeners[aTopic];

    // A null response indicates an error. If an error occurs in the callback
    // below, the response will remain null, and Java will fire onError for
    // this request.
    let response = null;

    try {
      let result = yield listener(wrapper.data);
      if (typeof result !== "object" || result === null) {
        throw new Error("Gecko request listener did not return an object");
      }
      response = result;
    } catch (e) {
      Cu.reportError(e);
    }

    sendMessageToJava({
      type: "Gecko:Request" + wrapper.id,
      response: response
    });
  })
};
