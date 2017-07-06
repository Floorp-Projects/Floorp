# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// We wastefully reload the same JS files across components.  This puts all
// the common JS files used by safebrowsing and url-classifier into a
// single component.

const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

#include ./content/moz/lang.js
#include ./content/request-backoff.js

// Wrap a general-purpose |RequestBackoff| to a v4-specific one
// since both listmanager and hashcompleter would use it.
// Note that |maxRequests| and |requestPeriod| is still configurable
// to throttle pending requests.
function RequestBackoffV4(maxRequests, requestPeriod) {
  let rand = Math.random();
  let retryInterval = Math.floor(15 * 60 * 1000 * (rand + 1));   // 15 ~ 30 min.
  let backoffInterval = Math.floor(30 * 60 * 1000 * (rand + 1)); // 30 ~ 60 min.

  return new RequestBackoff(2 /* max errors */,
                retryInterval /* retry interval, 15~30 min */,
                  maxRequests /* num requests */,
                requestPeriod /* request time, 60 min */,
              backoffInterval /* backoff interval, 60 min */,
          24 * 60 * 60 * 1000 /* max backoff, 24hr */);
}

// Expose this whole component.
var lib = this;

function UrlClassifierLib() {
  this.wrappedJSObject = lib;
}
UrlClassifierLib.prototype.classID = Components.ID("{26a4a019-2827-4a89-a85c-5931a678823a}");
UrlClassifierLib.prototype.QueryInterface = XPCOMUtils.generateQI([]);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UrlClassifierLib]);
