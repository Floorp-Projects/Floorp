/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

this.EXPORTED_SYMBOLS = ["ThirdPartyCookieProbe"];

const MILLISECONDS_PER_DAY = 1000 * 60 * 60 * 24;

/**
 * A probe implementing the measurements detailed at
 * https://wiki.mozilla.org/SecurityEngineering/ThirdPartyCookies/Telemetry
 *
 * This implementation uses only in-memory data.
 */
this.ThirdPartyCookieProbe = function() {
  /**
   * A set of third-party sites that have caused cookies to be
   * rejected. These sites are trimmed down to ETLD + 1
   * (i.e. "x.y.com" and "z.y.com" are both trimmed down to "y.com",
   * "x.y.co.uk" is trimmed down to "y.co.uk").
   *
   * Used to answer the following question: "For each third-party
   * site, how many other first parties embed them and result in
   * cookie traffic?" (see
   * https://wiki.mozilla.org/SecurityEngineering/ThirdPartyCookies/Telemetry#Breadth
   * )
   *
   * @type Map<string, RejectStats> A mapping from third-party site
   * to rejection statistics.
   */
  this._thirdPartyCookies = new Map();
  /**
   * Timestamp of the latest call to flush() in milliseconds since the Epoch.
   */
  this._latestFlush = Date.now();
};

this.ThirdPartyCookieProbe.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  init: function() {
    Services.obs.addObserver(this, "profile-before-change", false);
    Services.obs.addObserver(this, "third-party-cookie-accepted", false);
    Services.obs.addObserver(this, "third-party-cookie-rejected", false);
  },
  dispose: function() {
    Services.obs.removeObserver(this, "profile-before-change");
    Services.obs.removeObserver(this, "third-party-cookie-accepted");
    Services.obs.removeObserver(this, "third-party-cookie-rejected");
  },
  /**
   * Observe either
   * - "profile-before-change" (no meaningful subject or data) - time to flush statistics and unregister; or
   * - "third-party-cookie-accepted"/"third-party-cookie-rejected" with
   *    subject: the nsIURI of the third-party that attempted to set the cookie;
   *    data: a string holding the uri of the page seen by the user.
   */
  observe: function(docURI, topic, referrer) {
    try {
      if (topic == "profile-before-change") {
        // A final flush, then unregister
        this.flush();
        this.dispose();
      }
      if (topic != "third-party-cookie-accepted"
          && topic != "third-party-cookie-rejected") {
        // Not a third-party cookie
        return;
      }
      // Add host to this._thirdPartyCookies
      // Note: nsCookieService passes "?" if the issuer is unknown.  Avoid
      //       normalizing in this case since its not a valid URI.
      let firstParty = (referrer === "?") ? referrer : normalizeHost(referrer);
      let thirdParty = normalizeHost(docURI.QueryInterface(Ci.nsIURI).host);
      let data = this._thirdPartyCookies.get(thirdParty);
      if (!data) {
        data = new RejectStats();
        this._thirdPartyCookies.set(thirdParty, data);
      }
      if (topic == "third-party-cookie-accepted") {
        data.addAccepted(firstParty);
      } else {
        data.addRejected(firstParty);
      }
    } catch (ex) {
      if (ex instanceof Ci.nsIXPCException) {
        if (ex.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
            ex.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
          return;
        }
      }
      // Other errors should not remain silent.
      Services.console.logStringMessage("ThirdPartyCookieProbe: Uncaught error " + ex + "\n" + ex.stack);
    }
  },

  /**
   * Clear internal data, fill up corresponding histograms.
   *
   * @param {number} aNow (optional, used for testing purposes only)
   * The current instant. Used to make tests time-independent.
   */
  flush: function(aNow = Date.now()) {
    let updays = (aNow - this._latestFlush) / MILLISECONDS_PER_DAY;
    if (updays <= 0) {
      // Unlikely, but regardless, don't risk division by zero
      // or weird stuff.
      return;
    }
    this._latestFlush = aNow;
    let acceptedSites = Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_SITES_ACCEPTED");
    let rejectedSites = Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_SITES_BLOCKED");
    let acceptedRequests = Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_ATTEMPTS_ACCEPTED");
    let rejectedRequests = Services.telemetry.getHistogramById("COOKIES_3RDPARTY_NUM_ATTEMPTS_BLOCKED");
    for (let [k, data] of this._thirdPartyCookies) {
      acceptedSites.add(data.countAcceptedSites / updays);
      rejectedSites.add(data.countRejectedSites / updays);
      acceptedRequests.add(data.countAcceptedRequests / updays);
      rejectedRequests.add(data.countRejectedRequests / updays);
    }
    this._thirdPartyCookies.clear();
  }
};

/**
 * Data gathered on cookies that a third party site has attempted to set.
 *
 * Privacy note: the only data actually sent to the server is the size of
 * the sets.
 *
 * @constructor
 */
let RejectStats = function() {
  /**
   * The set of all sites for which we have accepted third-party cookies.
   */
  this._acceptedSites = new Set();
  /**
   * The set of all sites for which we have rejected third-party cookies.
   */
  this._rejectedSites = new Set();
  /**
   * Total number of attempts to set a third-party cookie that have
   * been accepted. Two accepted attempts on the same site will both
   * augment this count.
   */
  this._acceptedRequests = 0;
  /**
   * Total number of attempts to set a third-party cookie that have
   * been rejected. Two rejected attempts on the same site will both
   * augment this count.
   */
  this._rejectedRequests = 0;
};
RejectStats.prototype = {
  addAccepted: function(firstParty) {
    this._acceptedSites.add(firstParty);
    this._acceptedRequests++;
  },
  addRejected: function(firstParty) {
    this._rejectedSites.add(firstParty);
    this._rejectedRequests++;
  },
  get countAcceptedSites() {
    return this._acceptedSites.size;
  },
  get countRejectedSites() {
    return this._rejectedSites.size;
  },
  get countAcceptedRequests() {
    return this._acceptedRequests;
  },
  get countRejectedRequests() {
    return this._rejectedRequests;
  }
};

/**
 * Normalize a host to its eTLD + 1.
 */
function normalizeHost(host) {
  return Services.eTLD.getBaseDomainFromHost(host);
};
