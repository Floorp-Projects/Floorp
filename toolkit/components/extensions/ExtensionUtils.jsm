/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Run a function and report exceptions.
function runSafeSyncWithoutClone(f, ...args)
{
  try {
    return f(...args);
  } catch (e) {
    dump(`Extension error: ${e} ${e.fileName} ${e.lineNumber}\n[[Exception stack\n${e.stack}Current stack\n${Error().stack}]]\n`);
    Cu.reportError(e);
  }
}

// Run a function and report exceptions.
function runSafeWithoutClone(f, ...args)
{
  if (typeof(f) != "function") {
    dump(`Extension error: expected function\n${Error().stack}`);
    return;
  }

  Services.tm.currentThread.dispatch(function() {
    runSafeSyncWithoutClone(f, ...args);
  }, Ci.nsIEventTarget.DISPATCH_NORMAL);
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafeSync(context, f, ...args)
{
  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    dump(`runSafe failure\n${context.cloneScope}\n${Error().stack}`);
  }
  return runSafeSyncWithoutClone(f, ...args);
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafe(context, f, ...args)
{
  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    dump(`runSafe failure\n${context.cloneScope}\n${Error().stack}`);
  }
  return runSafeWithoutClone(f, ...args);
}

// Similar to a WeakMap, but returns a particular default value for
// |get| if a key is not present.
function DefaultWeakMap(defaultValue)
{
  this.defaultValue = defaultValue;
  this.weakmap = new WeakMap();
}

DefaultWeakMap.prototype = {
  get(key) {
    if (this.weakmap.has(key)) {
      return this.weakmap.get(key);
    }
    return this.defaultValue;
  },

  set(key, value) {
    if (key) {
      this.weakmap.set(key, value);
    } else {
      this.defaultValue = value;
    }
  },
};

// This is a generic class for managing event listeners. Example usage:
//
// new EventManager(context, "api.subAPI", fire => {
//   let listener = (...) => {
//     // Fire any listeners registered with addListener.
//     fire(arg1, arg2);
//   };
//   // Register the listener.
//   SomehowRegisterListener(listener);
//   return () => {
//     // Return a way to unregister the listener.
//     SomehowUnregisterListener(listener);
//   };
// }).api()
//
// The result is an object with addListener, removeListener, and
// hasListener methods. |context| is an add-on scope (either an
// ExtensionPage in the chrome process or ExtensionContext in a
// content process). |name| is for debugging. |register| is a function
// to register the listener. |register| is only called once, event if
// multiple listeners are registered. |register| should return an
// unregister function that will unregister the listener.
function EventManager(context, name, register)
{
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = null;
  this.callbacks = new Set();
  this.registered = false;
}

EventManager.prototype = {
  addListener(callback) {
    if (typeof(callback) != "function") {
      dump(`Expected function\n${Error().stack}`);
      return;
    }

    if (!this.registered) {
      this.context.callOnClose(this);

      let fireFunc = this.fire.bind(this);
      let fireWithoutClone = this.fireWithoutClone.bind(this);
      fireFunc.withoutClone = fireWithoutClone;
      this.unregister = this.register(fireFunc);
    }
    this.callbacks.add(callback);
  },

  removeListener(callback) {
    if (!this.registered) {
      return;
    }

    this.callbacks.delete(callback);
    if (this.callbacks.length == 0) {
      this.unregister();

      this.context.forgetOnClose(this);
    }
  },

  hasListener(callback) {
    return this.callbacks.has(callback);
  },

  fire(...args) {
    for (let callback of this.callbacks) {
      runSafe(this.context, callback, ...args);
    }
  },

  fireWithoutClone(...args) {
    for (let callback of this.callbacks) {
      runSafeSyncWithoutClone(callback, ...args);
    }
  },

  close() {
    this.unregister();
  },

  api() {
    return {
      addListener: callback => this.addListener(callback),
      removeListener: callback => this.removeListener(callback),
      hasListener: callback => this.hasListener(callback),
    };
  },
};

// Similar to EventManager, but it doesn't try to consolidate event
// notifications. Each addListener call causes us to register once. It
// allows extra arguments to be passed to addListener.
function SingletonEventManager(context, name, register)
{
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = new Map();
  context.callOnClose(this);
}

SingletonEventManager.prototype = {
  addListener(callback, ...args) {
    let unregister = this.register(callback, ...args);
    this.unregister.set(callback, unregister);
  },

  removeListener(callback) {
    if (!this.unregister.has(callback)) {
      return;
    }

    let unregister = this.unregister.get(callback);
    this.unregister.delete(callback);
    this.unregister();
  },

  hasListener(callback) {
    return this.unregister.has(callback);
  },

  close() {
    for (let unregister of this.unregister.values()) {
      unregister();
    }
  },

  api() {
    return {
      addListener: (...args) => this.addListener(...args),
      removeListener: (...args) => this.removeListener(...args),
      hasListener: (...args) => this.hasListener(...args),
    };
  },
};

// Simple API for event listeners where events never fire.
function ignoreEvent()
{
  return {
    addListener: function(context, callback) {},
    removeListener: function(context, callback) {},
    hasListener: function(context, callback) {},
  };
}

// Copy an API object from |source| into the scope |dest|.
function injectAPI(source, dest)
{
  for (let prop in source) {
    // Skip names prefixed with '_'.
    if (prop[0] == '_') {
      continue;
    }

    let value = source[prop];
    if (typeof(value) == "function") {
      Cu.exportFunction(value, dest, {defineAs: prop});
    } else if (typeof(value) == "object") {
      let obj = Cu.createObjectIn(dest, {defineAs: prop});
      injectAPI(value, obj);
    } else {
      dest[prop] = value;
    }
  }
}

/*
 * Messaging primitives.
 */

var nextBrokerId = 1;

var MESSAGES = [
  "Extension:Message",
  "Extension:Connect",
];

// Receives messages from multiple message managers and directs them
// to a set of listeners. On the child side: one broker per frame
// script.  On the parent side: one broker total, covering both the
// global MM and the ppmm. Message must be tagged with a recipient,
// which is an object with properties. Listeners can filter for
// messages that have a certain value for a particular property in the
// recipient. (If a message doesn't specify the given property, it's
// considered a match.)
function MessageBroker(messageManagers)
{
  this.messageManagers = messageManagers;
  for (let mm of this.messageManagers) {
    for (let message of MESSAGES) {
      mm.addMessageListener(message, this);
    }
  }

  this.listeners = {message: [], connect: []};
}

MessageBroker.prototype = {
  uninit() {
    for (let mm of this.messageManagers) {
      for (let message of MESSAGES) {
        mm.removeMessageListener(message, this);
      }
    }

    this.listeners = null;
  },

  makeId() {
    return nextBrokerId++;
  },

  addListener(type, listener, filter) {
    this.listeners[type].push({filter, listener});
  },

  removeListener(type, listener) {
    let index = -1;
    for (let i = 0; i < this.listeners[type].length; i++) {
      if (this.listeners[type][i].listener == listener) {
        this.listeners[type].splice(i, 1);
        return;
      }
    }
  },

  runListeners(type, target, data) {
    let listeners = [];
    for (let {listener, filter} of this.listeners[type]) {
      let pass = true;
      for (let prop in filter) {
        if (prop in data.recipient && filter[prop] != data.recipient[prop]) {
          pass = false;
          break;
        }
      }

      // Save up the list of listeners to call in case they modify the
      // set of listeners.
      if (pass) {
        listeners.push(listener);
      }
    }

    for (let listener of listeners) {
      listener(type, target, data.message, data.sender, data.recipient);
    }
  },

  receiveMessage({name, data, target}) {
    switch (name) {
    case "Extension:Message":
      this.runListeners("message", target, data);
      break;

    case "Extension:Connect":
      this.runListeners("connect", target, data);
      break;
    }
  },

  sendMessage(messageManager, type, message, sender, recipient) {
    let data = {message, sender, recipient};
    let names = {message: "Extension:Message", connect: "Extension:Connect"};
    messageManager.sendAsyncMessage(names[type], data);
  },
};

// Abstraction for a Port object in the extension API. Each port has a unique ID.
function Port(context, messageManager, name, id, sender)
{
  this.context = context;
  this.messageManager = messageManager;
  this.name = name;
  this.id = id;
  this.listenerName = `Extension:Port-${this.id}`;
  this.disconnectName = `Extension:Disconnect-${this.id}`;
  this.sender = sender;
  this.disconnected = false;

  this.messageManager.addMessageListener(this.disconnectName, this, true);
  this.disconnectListeners = new Set();
}

Port.prototype = {
  api() {
    let portObj = Cu.createObjectIn(this.context.cloneScope);

    // We want a close() notification when the window is destroyed.
    this.context.callOnClose(this);

    let publicAPI = {
      name: this.name,
      disconnect: () => {
        this.disconnect();
      },
      postMessage: json => {
        if (this.disconnected) {
          throw "Attempt to postMessage on disconnected port";
        }
        this.messageManager.sendAsyncMessage(this.listenerName, json);
      },
      onDisconnect: new EventManager(this.context, "Port.onDisconnect", fire => {
        let listener = () => {
          if (!this.disconnected) {
            fire();
          }
        };

        this.disconnectListeners.add(listener);
        return () => {
          this.disconnectListeners.delete(listener);
        };
      }).api(),
      onMessage: new EventManager(this.context, "Port.onMessage", fire => {
        let listener = ({data}) => {
          if (!this.disconnected) {
            fire(data);
          }
        };

        this.messageManager.addMessageListener(this.listenerName, listener);
        return () => {
          this.messageManager.removeMessageListener(this.listenerName, listener);
        };
      }).api(),
    };

    if (this.sender) {
      publicAPI.sender = this.sender;
    }

    injectAPI(publicAPI, portObj);
    return portObj;
  },

  handleDisconnection() {
    this.messageManager.removeMessageListener(this.disconnectName, this);
    this.context.forgetOnClose(this);
    this.disconnected = true;
  },

  receiveMessage(msg) {
    if (msg.name == this.disconnectName) {
      if (this.disconnected) {
        return;
      }

      for (let listener of this.disconnectListeners) {
        listener();
      }

      this.handleDisconnection();
    }
  },

  disconnect() {
    if (this.disconnected) {
      throw "Attempt to disconnect() a disconnected port";
    }
    this.handleDisconnection();
    this.messageManager.sendAsyncMessage(this.disconnectName);
  },

  close() {
    this.disconnect();
  },
};

function getMessageManager(target)
{
  if (target instanceof Ci.nsIDOMXULElement) {
    return target.messageManager;
  } else {
    return target;
  }
}

// Each extension scope gets its own Messenger object. It handles the
// basics of sendMessage, onMessage, connect, and onConnect.
//
// |context| is the extension scope.
// |broker| is a MessageBroker used to receive and send messages.
// |sender| is an object describing the sender (usually giving its extensionId, tabId, etc.)
// |filter| is a recipient filter to apply to incoming messages from the broker.
// |delegate| is an object that must implement a few methods:
//    getSender(context, messageManagerTarget, sender): returns a MessageSender
//      See https://developer.chrome.com/extensions/runtime#type-MessageSender.
function Messenger(context, broker, sender, filter, delegate)
{
  this.context = context;
  this.broker = broker;
  this.sender = sender;
  this.filter = filter;
  this.delegate = delegate;
}

Messenger.prototype = {
  sendMessage(messageManager, msg, recipient, responseCallback) {
    let id = this.broker.makeId();
    let replyName = `Extension:Reply-${id}`;
    recipient.messageId = id;
    this.broker.sendMessage(messageManager, "message", msg, this.sender, recipient);

    let onClose;
    let listener = ({data: response}) => {
      messageManager.removeMessageListener(replyName, listener);
      this.context.forgetOnClose(onClose);

      if (response.gotData) {
        // TODO: Handle failure to connect to the extension?
        runSafe(this.context, responseCallback, response.data);
      }
    };
    onClose = {
      close() {
        messageManager.removeMessageListener(replyName, listener);
      }
    };
    if (responseCallback) {
      messageManager.addMessageListener(replyName, listener);
      this.context.callOnClose(onClose);
    }
  },

  onMessage(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = (type, target, message, sender, recipient) => {
        message = Cu.cloneInto(message, this.context.cloneScope);
        if (this.delegate) {
          this.delegate.getSender(this.context, target, sender);
        }
        sender = Cu.cloneInto(sender, this.context.cloneScope);

        let mm = getMessageManager(target);
        let replyName = `Extension:Reply-${recipient.messageId}`;

        let valid = true, sent = false;
        let sendResponse = data => {
          if (!valid) {
            return;
          }
          sent = true;
          mm.sendAsyncMessage(replyName, {data, gotData: true});
        };
        sendResponse = Cu.exportFunction(sendResponse, this.context.cloneScope);

        let result = runSafeSyncWithoutClone(callback, message, sender, sendResponse);
        if (result !== true) {
          valid = false;
          if (!sent) {
            mm.sendAsyncMessage(replyName, {gotData: false});
          }
        }
      };

      this.broker.addListener("message", listener, this.filter);
      return () => {
        this.broker.removeListener("message", listener);
      };
    }).api();
  },

  connect(messageManager, name, recipient) {
    let portId = this.broker.makeId();
    let port = new Port(this.context, messageManager, name, portId, null);
    let msg = {name, portId};
    this.broker.sendMessage(messageManager, "connect", msg, this.sender, recipient);
    return port.api();
  },

  onConnect(name) {
    return new EventManager(this.context, name, fire => {
      let listener = (type, target, message, sender, recipient) => {
        let {name, portId} = message;
        let mm = getMessageManager(target);
        if (this.delegate) {
          this.delegate.getSender(this.context, target, sender);
        }
        let port = new Port(this.context, mm, name, portId, sender);
        fire.withoutClone(port.api());
      };

      this.broker.addListener("connect", listener, this.filter);
      return () => {
        this.broker.removeListener("connect", listener);
      };
    }).api();
  },
};

function flushJarCache(jarFile)
{
  Services.obs.notifyObservers(jarFile, "flush-cache-entry", null);
}

this.ExtensionUtils = {
  runSafeWithoutClone,
  runSafeSyncWithoutClone,
  runSafe,
  runSafeSync,
  DefaultWeakMap,
  EventManager,
  SingletonEventManager,
  ignoreEvent,
  injectAPI,
  MessageBroker,
  Messenger,
  flushJarCache,
};

