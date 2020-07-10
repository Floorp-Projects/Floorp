/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const protocolHandler = Cc[
  "@mozilla.org/network/protocol;1?name=http"
].getService(Ci.nsIHttpProtocolHandler);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const TLS_ERROR_REPORT_TELEMETRY_SUCCESS = 6;
const TLS_ERROR_REPORT_TELEMETRY_FAILURE = 7;
const HISTOGRAM_ID = "TLS_ERROR_REPORT_UI";

ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

function SecurityReporter() {}

SecurityReporter.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsISecurityReporter"]),
  reportTLSError(transportSecurityInfo, hostname, port) {
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
    let endpoint = Services.prefs.getCharPref(
      "security.ssl.errorReporting.url"
    );
    let reportURI = Services.io.newURI(endpoint);

    if (reportURI.host == hostname) {
      return;
    }

    // Convert the array of nsIX509Cert into a format that can be parsed into
    // JSON
    let asciiCertChain = [];

    if (transportSecurityInfo.failedCertChain) {
      for (let cert of transportSecurityInfo.failedCertChain) {
        asciiCertChain.push(cert.getBase64DERString());
      }
    }

    let report = {
      hostname,
      port,
      timestamp: Math.round(Date.now() / 1000),
      errorCode: transportSecurityInfo.errorCode,
      failedCertChain: asciiCertChain,
      userAgent: protocolHandler.userAgent,
      version: 1,
      build: Services.appinfo.appBuildID,
      product: Services.appinfo.name,
      channel: UpdateUtils.UpdateChannel,
    };

    fetch(endpoint, {
      method: "POST",
      body: JSON.stringify(report),
      credentials: "omit",
      headers: {
        "Content-Type": "application/json",
      },
    })
      .then(function(aResponse) {
        if (!aResponse.ok) {
          // request returned non-success status
          Services.telemetry
            .getHistogramById(HISTOGRAM_ID)
            .add(TLS_ERROR_REPORT_TELEMETRY_FAILURE);
        } else {
          Services.telemetry
            .getHistogramById(HISTOGRAM_ID)
            .add(TLS_ERROR_REPORT_TELEMETRY_SUCCESS);
        }
      })
      .catch(function(e) {
        // error making request to reportURL
        Services.telemetry
          .getHistogramById(HISTOGRAM_ID)
          .add(TLS_ERROR_REPORT_TELEMETRY_FAILURE);
      });
  },
};

var EXPORTED_SYMBOLS = ["SecurityReporter"];
