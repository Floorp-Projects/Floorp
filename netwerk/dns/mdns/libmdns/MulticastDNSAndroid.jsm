// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MulticastDNS"];

const { EventDispatcher } = ChromeUtils.import(
  "resource://gre/modules/Messaging.jsm"
);

const DEBUG = false;

var log = function(s) {};
if (DEBUG) {
  log = ChromeUtils.import(
    "resource://gre/modules/AndroidLog.jsm",
    {}
  ).AndroidLog.d.bind(null, "MulticastDNS");
}

const FAILURE_INTERNAL_ERROR = -65537;

// Helper function for sending commands to Java.
function send(type, data, callback) {
  let msg = {
    type,
  };

  for (let i in data) {
    try {
      msg[i] = data[i];
    } catch (e) {}
  }

  EventDispatcher.instance
    .sendRequestForResult(msg)
    .then(
      result => callback(result, null),
      err =>
        callback(null, typeof err === "number" ? err : FAILURE_INTERNAL_ERROR)
    );
}

// Receives service found/lost event from NsdManager
function ServiceManager() {}

ServiceManager.prototype = {
  listeners: {},
  numListeners: 0,

  onEvent(event, data, callback) {
    if (event === "NsdManager:ServiceFound") {
      this.onServiceFound();
    } else if (event === "NsdManager:ServiceLost") {
      this.onServiceLost();
    }
  },

  registerEvent() {
    log("registerEvent");
    EventDispatcher.instance.registerListener(this, [
      "NsdManager:ServiceFound",
      "NsdManager:ServiceLost",
    ]);
  },

  unregisterEvent() {
    log("unregisterEvent");
    EventDispatcher.instance.unregisterListener(this, [
      "NsdManager:ServiceFound",
      "NsdManager:ServiceLost",
    ]);
  },

  addListener(aServiceType, aListener) {
    log("addListener: " + aServiceType + ", " + aListener);

    if (!this.listeners[aServiceType]) {
      this.listeners[aServiceType] = [];
    }
    if (this.listeners[aServiceType].includes(aListener)) {
      log("listener already exists");
      return;
    }

    this.listeners[aServiceType].push(aListener);
    ++this.numListeners;

    if (this.numListeners === 1) {
      this.registerEvent();
    }

    log("listener added: " + this);
  },

  removeListener(aServiceType, aListener) {
    log("removeListener: " + aServiceType + ", " + aListener);

    if (!this.listeners[aServiceType]) {
      log("listener doesn't exist");
      return;
    }
    let index = this.listeners[aServiceType].indexOf(aListener);
    if (index < 0) {
      log("listener doesn't exist");
      return;
    }

    this.listeners[aServiceType].splice(index, 1);
    --this.numListeners;

    if (this.numListeners === 0) {
      this.unregisterEvent();
    }

    log("listener removed" + this);
  },

  onServiceFound(aServiceInfo) {
    let listeners = this.listeners[aServiceInfo.serviceType];
    if (listeners) {
      for (let listener of listeners) {
        listener.onServiceFound(aServiceInfo);
      }
    } else {
      log("no listener");
    }
    return {};
  },

  onServiceLost(aServiceInfo) {
    let listeners = this.listeners[aServiceInfo.serviceType];
    if (listeners) {
      for (let listener of listeners) {
        listener.onServiceLost(aServiceInfo);
      }
    } else {
      log("no listener");
    }
    return {};
  },
};

// make an object from nsIPropertyBag2
function parsePropertyBag2(bag) {
  if (!bag || !(bag instanceof Ci.nsIPropertyBag2)) {
    throw new TypeError("Not a property bag");
  }

  let attributes = [];
  for (let { name } of bag.enumerator) {
    let value = bag.getPropertyAsACString(name);
    attributes.push({
      name,
      value,
    });
  }

  return attributes;
}

function MulticastDNS() {
  this.serviceManager = new ServiceManager();
}

MulticastDNS.prototype = {
  startDiscovery(aServiceType, aListener) {
    this.serviceManager.addListener(aServiceType, aListener);

    let serviceInfo = {
      serviceType: aServiceType,
      uniqueId: aListener.uuid,
    };

    send("NsdManager:DiscoverServices", serviceInfo, (result, err) => {
      if (err) {
        log("onStartDiscoveryFailed: " + aServiceType + " (" + err + ")");
        this.serviceManager.removeListener(aServiceType, aListener);
        aListener.onStartDiscoveryFailed(aServiceType, err);
      } else {
        aListener.onDiscoveryStarted(result);
      }
    });
  },

  stopDiscovery(aServiceType, aListener) {
    this.serviceManager.removeListener(aServiceType, aListener);

    let serviceInfo = {
      uniqueId: aListener.uuid,
    };

    send("NsdManager:StopServiceDiscovery", serviceInfo, (result, err) => {
      if (err) {
        log("onStopDiscoveryFailed: " + aServiceType + " (" + err + ")");
        aListener.onStopDiscoveryFailed(aServiceType, err);
      } else {
        aListener.onDiscoveryStopped(aServiceType);
      }
    });
  },

  registerService(aServiceInfo, aListener) {
    let serviceInfo = {
      port: aServiceInfo.port,
      serviceType: aServiceInfo.serviceType,
      uniqueId: aListener.uuid,
    };

    try {
      serviceInfo.host = aServiceInfo.host;
    } catch (e) {
      // host unspecified
    }
    try {
      serviceInfo.serviceName = aServiceInfo.serviceName;
    } catch (e) {
      // serviceName unspecified
    }
    try {
      serviceInfo.attributes = parsePropertyBag2(aServiceInfo.attributes);
    } catch (e) {
      // attributes unspecified
    }

    send("NsdManager:RegisterService", serviceInfo, (result, err) => {
      if (err) {
        log("onRegistrationFailed: (" + err + ")");
        aListener.onRegistrationFailed(aServiceInfo, err);
      } else {
        aListener.onServiceRegistered(result);
      }
    });
  },

  unregisterService(aServiceInfo, aListener) {
    let serviceInfo = {
      uniqueId: aListener.uuid,
    };

    send("NsdManager:UnregisterService", serviceInfo, (result, err) => {
      if (err) {
        log("onUnregistrationFailed: (" + err + ")");
        aListener.onUnregistrationFailed(aServiceInfo, err);
      } else {
        aListener.onServiceUnregistered(aServiceInfo);
      }
    });
  },

  resolveService(aServiceInfo, aListener) {
    send("NsdManager:ResolveService", aServiceInfo, (result, err) => {
      if (err) {
        log("onResolveFailed: (" + err + ")");
        aListener.onResolveFailed(aServiceInfo, err);
      } else {
        aListener.onServiceResolved(result);
      }
    });
  },
};
