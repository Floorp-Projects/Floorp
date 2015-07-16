/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/MulticastDNS.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const DNSSERVICEDISCOVERY_CID = Components.ID("{f9346d98-f27a-4e89-b744-493843416480}");
const DNSSERVICEDISCOVERY_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-sd;1";
const DNSSERVICEINFO_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-info;1";

function log(aMsg) {
  dump("-*- nsDNSServiceDiscovery.js : " + aMsg + "\n");
}

// Helper class to transform return objects to correct type.
function ListenerWrapper(aListener) {
  this.listener = aListener;
}

ListenerWrapper.prototype = {
  // Helper function for transforming an Object into nsIDNSServiceInfo.
  makeServiceInfo: function (aServiceInfo) {
    let serviceInfo = Cc[DNSSERVICEINFO_CONTRACT_ID].createInstance(Ci.nsIDNSServiceInfo);

    for (let name of ['host', 'port', 'serviceName', 'serviceType']) {
      try {
        serviceInfo[name] = aServiceInfo[name];
      } catch (e) {
        // ignore exceptions
      }
    }

    return serviceInfo;
  },

  /* transparent types */
  onDiscoveryStarted: function(aServiceType) {
    this.listener.onDiscoveryStarted(aServiceType);
  },
  onDiscoveryStopped: function(aServiceType) {
    this.listener.onDiscoveryStopped(aServiceType);
  },
  onStartDiscoveryFailed: function(aServiceType, aErrorCode) {
    this.listener.onStartDiscoveryFailed(aServiceType);
  },
  onStopDiscoveryFailed: function(aServiceType, aErrorCode) {
    this.listener.onStopDiscoveryFailed(aServiceType);
  },

  /* transform types */
  onServiceFound: function(aServiceInfo) {
    this.listener.onServiceFound(this.makeServiceInfo(aServiceInfo));
  },
  onServiceLost: function(aServiceInfo) {
    this.listener.onServiceLost(this.makeServiceInfo(aServiceInfo));
  },
  onServiceRegistered: function(aServiceInfo) {
    this.listener.onServiceRegistered(this.makeServiceInfo(aServiceInfo));
  },
  onServiceUnregistered: function(aServiceInfo) {
    this.listener.onServiceUnregistered(this.makeServiceInfo(aServiceInfo));
  },
  onServiceResolved: function(aServiceInfo) {
    this.listener.onServiceResolved(this.makeServiceInfo(aServiceInfo));
  },

  onRegistrationFailed: function(aServiceInfo, aErrorCode) {
    this.listener.onRegistrationFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  },
  onUnregistrationFailed: function(aServiceInfo, aErrorCode) {
    this.listener.onUnregistrationFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  },
  onResolveFailed: function(aServiceInfo, aErrorCode) {
    this.listener.onResolveFailed(this.makeServiceInfo(aServiceInfo), aErrorCode);
  }
};

function nsDNSServiceDiscovery() {
  log("nsDNSServiceDiscovery");
  this.mdns = new MulticastDNS();
}

nsDNSServiceDiscovery.prototype = {
  classID: DNSSERVICEDISCOVERY_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference, Ci.nsIDNSServiceDiscovery]),

  startDiscovery: function(aServiceType, aListener) {
    log("startDiscovery");
    let listener = new ListenerWrapper(aListener);
    this.mdns.startDiscovery(aServiceType, listener);

    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
      cancel: (function() {
        this.mdns.stopDiscovery(aServiceType, listener);
      }).bind(this)
    };
  },

  registerService: function(aServiceInfo, aListener) {
    log("registerService");
    let listener = new ListenerWrapper(aListener);
    this.mdns.registerService(aServiceInfo, listener);

    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
      cancel: (function() {
        this.mdns.unregisterService(aServiceInfo, listener);
      }).bind(this)
    };
  },

  resolveService: function(aServiceInfo, aListener) {
    log("resolveService");
    this.mdns.resolveService(aServiceInfo, new ListenerWrapper(aListener));
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsDNSServiceDiscovery]);
