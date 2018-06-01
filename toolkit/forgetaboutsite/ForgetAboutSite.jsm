/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var EXPORTED_SYMBOLS = ["ForgetAboutSite"];

var ForgetAboutSite = {
  async removeDataFromDomain(aDomain) {
    let promises = [];

    ["http://", "https://"].forEach(scheme => {
      promises.push(new Promise(resolve => {
        Services.clearData.deleteDataFromHost(aDomain, true /* user request */,
                                              Ci.nsIClearDataService.CLEAR_ALL,
                                              resolve);
      }));
    });

    // EME
    promises.push((async function() {
      let mps = Cc["@mozilla.org/gecko-media-plugin-service;1"].
                getService(Ci.mozIGeckoMediaPluginChromeService);
      mps.forgetThisSite(aDomain, JSON.stringify({}));
    })().catch(ex => {
      throw new Error("Exception thrown while clearing Encrypted Media Extensions: " + ex);
    }));

    // HSTS and HPKP
    promises.push((async function() {
      let sss = Cc["@mozilla.org/ssservice;1"].
                getService(Ci.nsISiteSecurityService);
      for (let type of [Ci.nsISiteSecurityService.HEADER_HSTS,
                        Ci.nsISiteSecurityService.HEADER_HPKP]) {
        // Also remove HSTS/HPKP information for subdomains by enumerating the
        // information in the site security service.
        let enumerator = sss.enumerate(type);
        while (enumerator.hasMoreElements()) {
          let entry = enumerator.getNext();
          let hostname = entry.QueryInterface(Ci.nsISiteSecurityState).hostname;
          // If the hostname is aDomain's subdomain, we remove its state.
          if (hostname == aDomain || hostname.endsWith("." + aDomain)) {
            // This uri is used as a key to remove the state.
            let uri = NetUtil.newURI("https://" + hostname);
            sss.removeState(type, uri, 0, entry.originAttributes);
          }
        }
      }
    })().catch(ex => {
      throw new Error("Exception thrown while clearing HSTS/HPKP: " + ex);
    }));

    let ErrorCount = 0;
    for (let promise of promises) {
      try {
        await promise;
      } catch (ex) {
        Cu.reportError(ex);
        ErrorCount++;
      }
    }
    if (ErrorCount !== 0)
      throw new Error(`There were a total of ${ErrorCount} errors during removal`);
  }
};
