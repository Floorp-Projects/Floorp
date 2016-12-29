/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.importGlobalProperties(['fetch']);

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const protocolHandler = Cc["@mozilla.org/network/protocol;1?name=http"]
                          .getService(Ci.nsIHttpProtocolHandler);
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const TLS_ERROR_REPORT_TELEMETRY_SUCCESS = 6;
const TLS_ERROR_REPORT_TELEMETRY_FAILURE = 7;
const HISTOGRAM_ID = "TLS_ERROR_REPORT_UI";


XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");

function getDERString(cert)
{
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = '';
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function SecurityReporter() { }

SecurityReporter.prototype = {
  classDescription: "Security reporter component",
  classID:          Components.ID("{8a997c9a-bea1-11e5-a1fa-be6aBc8e7f8b}"),
  contractID:       "@mozilla.org/securityreporter;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISecurityReporter]),
  reportTLSError: function(transportSecurityInfo, hostname, port) {
    // don't send if there's no transportSecurityInfo (since the report cannot
    // contain anything of interest)
    if (!transportSecurityInfo) {
      return;
    }

    // don't send a report if the pref is not enabled
    if (!Services.prefs.getBoolPref("security.ssl.errorReporting.enabled")) {
      return;
    }

    // Don't send a report if the host we're connecting to is the report
    // server (otherwise we'll get loops when this fails)
    let endpoint =
      Services.prefs.getCharPref("security.ssl.errorReporting.url");
    let reportURI = Services.io.newURI(endpoint, null, null);

    if (reportURI.host == hostname) {
      return;
    }

    // Convert the nsIX509CertList into a format that can be parsed into
    // JSON
    let asciiCertChain = [];

    if (transportSecurityInfo.failedCertChain) {
      let certs = transportSecurityInfo.failedCertChain.getEnumerator();
      while (certs.hasMoreElements()) {
        let cert = certs.getNext();
        cert.QueryInterface(Ci.nsIX509Cert);
        asciiCertChain.push(btoa(getDERString(cert)));
      }
    }

    let report = {
      hostname: hostname,
      port: port,
      timestamp: Math.round(Date.now() / 1000),
      errorCode: transportSecurityInfo.errorCode,
      failedCertChain: asciiCertChain,
      userAgent: protocolHandler.userAgent,
      version: 1,
      build: Services.appinfo.appBuildID,
      product: Services.appinfo.name,
      channel: UpdateUtils.UpdateChannel
    }

    fetch(endpoint, {
      method: "POST",
      body: JSON.stringify(report),
      headers: {
        'Content-Type': 'application/json'
      }
    }).then(function(aResponse) {
      if (!aResponse.ok) {
        // request returned non-success status
        Services.telemetry.getHistogramById(HISTOGRAM_ID)
          .add(TLS_ERROR_REPORT_TELEMETRY_FAILURE);
      } else {
        Services.telemetry.getHistogramById(HISTOGRAM_ID)
          .add(TLS_ERROR_REPORT_TELEMETRY_SUCCESS);
      }
    }).catch(function(e) {
      // error making request to reportURL
      Services.telemetry.getHistogramById(HISTOGRAM_ID)
          .add(TLS_ERROR_REPORT_TELEMETRY_FAILURE);
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SecurityReporter]);
