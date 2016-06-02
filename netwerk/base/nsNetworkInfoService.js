/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://gre/modules/NetworkInfoServiceAndroid.jsm");

const NETWORKINFOSERVICE_CID = Components.ID("{0d8245af-d5d3-4100-a6b5-1cc026aa19f2}");
const NETWORKINFOSERVICE_CONTRACT_ID = "@mozilla.org/network-info-service;1";

function log(aMsg) {
  dump("-*- nsNetworkInfoService.js : " + aMsg + "\n");
}

function setQueryInterface(cls, ...aQIList) {
  cls.prototype.QueryInterface = XPCOMUtils.generateQI(aQIList);
}

class nsNetworkInfoService {
  constructor() {
    this.impl = new NetworkInfoServiceAndroid();
    log("nsNetworkInfoService");
  }

  listNetworkAddresses(aListener) {
    this.impl.listNetworkAddresses({
        onListNetworkAddressesFailed(err) {
            aListener.onListNetworkAddressesFailed();
        },
        onListedNetworkAddresses(addresses) {
            aListener.onListedNetworkAddresses(addresses, addresses.length);
        }
    });
  }

  getHostname(aListener) {
    this.impl.getHostname({
        onGetHostnameFailed(err) {
            aListener.onGetHostnameFailed();
        },
        onGotHostname(hostname) {
            aListener.onGotHostname(hostname);
        }
    });
  }
}
setQueryInterface(nsNetworkInfoService, Ci.nsISupportsWeakReference,
                  Ci.nsINetworkInfoService);
nsNetworkInfoService.prototype.classID = NETWORKINFOSERVICE_CID;

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsNetworkInfoService]);
