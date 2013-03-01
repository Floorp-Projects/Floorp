/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/UserAgentOverrides.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
                     .getService(Ci.nsIHttpProtocolHandler)
                     .userAgent;

function SiteSpecificUserAgent() {}

SiteSpecificUserAgent.prototype = {
  getUserAgentForURIAndWindow: function ssua_getUserAgentForURIAndWindow(aURI, aWindow) {
    let win = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator).getMostRecentWindow("navigator:browser");
    if (win && win.UserAgent) {
      return win.UserAgent.getUserAgentForWindow(aWindow, DEFAULT_UA);
    }
    return DEFAULT_UA;
  },

  classID: Components.ID("{d5234c9d-0ee2-4b3c-9da3-18be9e5cf7e6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISiteSpecificUserAgent])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SiteSpecificUserAgent]);
