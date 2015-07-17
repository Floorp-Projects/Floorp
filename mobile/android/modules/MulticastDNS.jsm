// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MulticastDNS"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
let log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "MulticastDNS");

// Helper function for sending commands to Java.
function send(type, data, callback) {
  let msg = {
    type: type
  };

  for (let i in data) {
    try {
      msg[i] = data[i];
    } catch (e) {
    }
  }

  Messaging.sendRequestForResult(msg)
    .then(result => callback(result, null), err => callback(null, err));
}

// Receives service found/lost event from NsdManager
function ServiceManager() {
}

ServiceManager.prototype = {
  listeners: {},
  numListeners: 0,

  registerEvent: function() {
    log("registerEvent");
    Messaging.addListener(this.onServiceFound.bind(this), "NsdManager:ServiceFound");
    Messaging.addListener(this.onServiceLost.bind(this), "NsdManager:ServiceLost");
  },

  unregisterEvent: function() {
    log("unregisterEvent");
    Messaging.removeListener("NsdManager:ServiceFound");
    Messaging.removeListener("NsdManager:ServiceLost");
  },

  addListener: function(aServiceType, aListener) {
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

  removeListener: function(aServiceType, aListener) {
    log("removeListener: " + aServiceType + ", " + aListener);

    if (!this.listeners[aServiceType]) {
      log("listener doesn't exist");
      return;
    }
    let index = this.listeners[aServiceType].indexOf(aListener);
    if (index < 0) {
      return;
    }

    this.listeners[aServiceType].splice(index, 1);
    --this.numListeners;

    if (this.numListeners === 0) {
      this.unregisterEvent();
    }

    log("listener removed" + this);
  },

  onServiceFound: function(aServiceInfo) {
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

  onServiceLost: function(aServiceInfo) {
    let listeners = this.listeners[aServiceInfo.serviceType];
    if (listeners) {
      for (let listener of listeners) {
        listener.onServiceLost(aServiceInfo);
      }
    } else {
      log("no listener");
    }
    return {};
  }
};

// make an object from nsIPropertyBag2
function parsePropertyBag2(bag) {
  if (!bag || !(bag instanceof Ci.nsIPropertyBag2)) {
    throw new TypeError("Not a property bag");
  }

  let attributes = [];
  let enumerator = bag.enumerator;
  while (enumerator.hasMoreElements()) {
    let name = enumerator.getNext().QueryInterface(Ci.nsIProperty).name;
    let value = bag.getPropertyAsACString(name);
    attributes.push({
      "name": name,
      "value": value
    });
  }

  return attributes;
}

function MulticastDNS() {
  this.serviceManager = new ServiceManager();
}

MulticastDNS.prototype = {
  startDiscovery: function(aServiceType, aListener) {
    this.serviceManager.addListener(aServiceType, aListener);

    send("NsdManager:DiscoverServices", { serviceType: aServiceType }, (result, err) => {
      if (err) {
        log("onStartDiscoveryFailed: " + aServiceType + " (" + err + ")");
        this.unregisterEvent();
        aListener.onStartDiscoveryFailed(aServiceType, err);
      } else {
        aListener.onDiscoveryStarted(result);
      }
    });
  },

  stopDiscovery: function(aServiceType, aListener) {
    this.serviceManager.removeListener(aServiceType, aListener);

    send("NsdManager:StopServiceDiscovery", {}, (result, err) => {
      if (err) {
        log("onStopDiscoveryFailed: " + aServiceType + " (" + err + ")");
        aListener.onStopDiscoveryFailed(aServiceType, err);
      } else {
        aListener.onDiscoveryStopped(aServiceType);
      }
    });
  },

  registerService: function(aServiceInfo, aListener) {
    let serviceInfo = {
      port: aServiceInfo.port,
      serviceType: aServiceInfo.serviceType,
    };

    try {
      serviceInfo.host = aServiceInfo.host;
    } catch(e) {
      // host unspecified
    }
    try {
      serviceInfo.serviceName = aServiceInfo.serviceName;
    } catch(e) {
      // serviceName unspecified
    }
    try {
      serviceInfo.attributes = parsePropertyBag2(aServiceInfo.attributes);
    } catch(e) {
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

  unregisterService: function(aServiceInfo, aListener) {
    send("NsdManager:UnregisterService", {}, (result, err) => {
      if (err) {
        log("onUnregistrationFailed: (" + err + ")");
        aListener.onUnregistrationFailed(aServiceInfo, err);
      } else {
        aListener.onServiceUnregistered(aServiceInfo);
      }
    });
  },

  resolveService: function(aServiceInfo, aListener) {
    send("NsdManager:ResolveService", aServiceInfo, (result, err) => {
      if (err) {
        log("onResolveFailed: (" + err + ")");
        aListener.onResolveFailed(aServiceInfo, err);
      } else {
        aListener.onServiceResolved(result);
      }
    });
  }
};
