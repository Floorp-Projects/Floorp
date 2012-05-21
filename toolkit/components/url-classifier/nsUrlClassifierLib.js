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
#include ./content/moz/preferences.js
#include ./content/moz/debug.js
#include ./content/moz/alarm.js
#include ./content/moz/cryptohasher.js
#include ./content/moz/observer.js
#include ./content/moz/protocol4.js

#include ./content/request-backoff.js
#include ./content/url-crypto-key-manager.js
#include ./content/xml-fetcher.js

// Expose this whole component.
var lib = this;

function UrlClassifierLib() {
  this.wrappedJSObject = lib;
}
UrlClassifierLib.prototype.classID = Components.ID("{26a4a019-2827-4a89-a85c-5931a678823a}");
UrlClassifierLib.prototype.QueryInterface = XPCOMUtils.generateQI([]);

var NSGetFactory = XPCOMUtils.generateNSGetFactory([UrlClassifierLib]);
