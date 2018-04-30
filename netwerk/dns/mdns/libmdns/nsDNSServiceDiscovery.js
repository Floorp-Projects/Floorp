/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import('resource://gre/modules/Services.jsm');
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

if (AppConstants.platform == "android" && !Services.prefs.getBoolPref("network.mdns.use_js_fallback")) {
  ChromeUtils.import("resource://gre/modules/MulticastDNSAndroid.jsm");
} else {
  ChromeUtils.import("resource://gre/modules/MulticastDNS.jsm");
}

const DNSSERVICEDISCOVERY_CID = Components.ID("{f9346d98-f27a-4e89-b744-493843416480}");
const DNSSERVICEDISCOVERY_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-sd;1";
const DNSSERVICEINFO_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-info;1";

function log(aMsg) {
  dump("-*- nsDNSServiceDiscovery.js : " + aMsg + "\n");
}

function generateUuid() {
  var uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].
    getService(Ci.nsIUUIDGenerator);
  return uuidGenerator.generateUUID().toString();
}

// Helper class to transform return objects to correct type.
function ListenerWrapper(aListener, aMdns) {
  this.listener = aListener;
  this.mdns = aMdns;

  this.discoveryStarting = false;
  this.stopDiscovery = false;

  this.registrationStarting = false;
  this.stopRegistration = false;

  this.uuid = generateUuid();
}

ListenerWrapper.prototype = {
  // Helper function for transforming an Object into nsIDNSServiceInfo.
  makeServiceInfo: function (aServiceInfo) {
    let serviceInfo = Cc[DNSSERVICEINFO_CONTRACT_ID].createInstance(Ci.nsIDNSServiceInfo);

    for (let name of ['host', 'address', 'port', 'serviceName', 'serviceType']) {
      try {
        serviceInfo[name] = aServiceInfo[name];
      } catch (e) {
        // ignore exceptions
      }
    }

    let attributes;
    try {
      attributes = _toPropertyBag2(aServiceInfo.attributes);
    } catch (err) {
        // Ignore unset attributes in object.
        log("Caught unset attributes error: " + err + " - " + err.stack);
        attributes = Cc['@mozilla.org/hash-property-bag;1']
                        .createInstance(Ci.nsIWritablePropertyBag2);
    }
    serviceInfo.attributes = attributes;

    return serviceInfo;
  },

  /* transparent types */
  onDiscoveryStarted: function(aServiceType) {
    this.discoveryStarting = false;
    this.listener.onDiscoveryStarted(aServiceType);

    if (this.stopDiscovery) {
      this.mdns.stopDiscovery(aServiceType, this);
    }
  },
  onDiscoveryStopped: function(aServiceType) {
    this.listener.onDiscoveryStopped(aServiceType);
  },
  onStartDiscoveryFailed: function(aServiceType, aErrorCode) {
    log('onStartDiscoveryFailed: ' + aServiceType + ' (' + aErrorCode + ')');
    this.discoveryStarting = false;
    this.stopDiscovery = true;
    this.listener.onStartDiscoveryFailed(aServiceType, aErrorCode);
  },
  onStopDiscoveryFailed: function(aServiceType, aErrorCode) {
    log('onStopDiscoveryFailed: ' + aServiceType + ' (' + aErrorCode + ')');
    this.listener.onStopDiscoveryFailed(aServiceType, aErrorCode);
  },

  /* transform types */
  onServiceFound: function(aServiceInfo) {
    this.listener.onServiceFound(this.makeServiceInfo(aServiceInfo));
  },
  onServiceLost: function(aServiceInfo) {
    this.listener.onServiceLost(this.makeServiceInfo(aServiceInfo));
  },
  onServiceRegistered: function(aServiceInfo) {
    this.registrationStarting = false;
    this.listener.onServiceRegistered(this.makeServiceInfo(aServiceInfo));

    if (this.stopRegistration) {
      this.mdns.unregisterService(aServiceInfo, this);
    }
  },
  onServiceUnregistered: function(aServiceInfo) {
    this.listener.onServiceUnregistered(this.makeServiceInfo(aServiceInfo));
  },
  onServiceResolved: function(aServiceInfo) {
    this.listener.onServiceResolved(this.makeServiceInfo(aServiceInfo));
  },

  onRegistrationFailed: function(aServiceInfo, aErrorCode) {
    log('onRegistrationFailed: (' + aErrorCode + ')');
    this.registrationStarting = false;
    this.stopRegistration = true;
    this.listener.onRegistrationFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  },
  onUnregistrationFailed: function(aServiceInfo, aErrorCode) {
    log('onUnregistrationFailed: (' + aErrorCode + ')');
    this.listener.onUnregistrationFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  },
  onResolveFailed: function(aServiceInfo, aErrorCode) {
    log('onResolveFailed: (' + aErrorCode + ')');
    this.listener.onResolveFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  }
};

function nsDNSServiceDiscovery() {
  log("nsDNSServiceDiscovery");
  this.mdns = new MulticastDNS();
}

nsDNSServiceDiscovery.prototype = {
  classID: DNSSERVICEDISCOVERY_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsISupportsWeakReference, Ci.nsIDNSServiceDiscovery]),

  startDiscovery: function(aServiceType, aListener) {
    log("startDiscovery");
    let listener = new ListenerWrapper(aListener, this.mdns);
    listener.discoveryStarting = true;
    this.mdns.startDiscovery(aServiceType, listener);

    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICancelable]),
      cancel: (function() {
        if (this.discoveryStarting || this.stopDiscovery) {
          this.stopDiscovery = true;
          return;
        }
        this.mdns.stopDiscovery(aServiceType, listener);
      }).bind(listener)
    };
  },

  registerService: function(aServiceInfo, aListener) {
    log("registerService");
    let listener = new ListenerWrapper(aListener, this.mdns);
    listener.registrationStarting = true;
    this.mdns.registerService(aServiceInfo, listener);

    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICancelable]),
      cancel: (function() {
        if (this.registrationStarting || this.stopRegistration) {
          this.stopRegistration = true;
          return;
        }
        this.mdns.unregisterService(aServiceInfo, listener);
      }).bind(listener)
    };
  },

  resolveService: function(aServiceInfo, aListener) {
    log("resolveService");
    this.mdns.resolveService(aServiceInfo, new ListenerWrapper(aListener));
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsDNSServiceDiscovery]);

function _toPropertyBag2(obj)
{
  if (obj.QueryInterface) {
    return obj.QueryInterface(Ci.nsIPropertyBag2);
  }

  let result = Cc['@mozilla.org/hash-property-bag;1']
                  .createInstance(Ci.nsIWritablePropertyBag2);
  for (let name in obj) {
    result.setPropertyAsAString(name, obj[name]);
  }
  return result;
}
