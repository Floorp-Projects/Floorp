/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function mozProtocolHandler() {
  XPCOMUtils.defineLazyPreferenceGetter(this, "urlToLoad", "toolkit.mozprotocol.url",
                                        "https://www.mozilla.org/protocol");
}

mozProtocolHandler.prototype = {
  scheme: "moz",
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD,

  newURI(spec, charset, base) {
    let mutator = Cc["@mozilla.org/network/simple-uri-mutator;1"]
                    .createInstance(Ci.nsIURIMutator);
    if (base) {
      mutator.setSpec(base.resolve(spec));
    } else {
      mutator.setSpec(spec);
    }
    return mutator.finalize();
  },

  newChannel2(uri, loadInfo) {
    const kCanada = "https://www.mozilla.org/contact/communities/canada/";
    let realURL = NetUtil.newURI((uri && uri.spec == "moz://eh") ? kCanada : this.urlToLoad);
    let channel = Services.io.newChannelFromURIWithLoadInfo(realURL, loadInfo);
    loadInfo.resultPrincipalURI = realURL;
    return channel;
  },

  newChannel(uri) {
    return this.newChannel2(uri, null);
  },

  classID: Components.ID("{47a45e5f-691e-4799-8686-14f8d3fc0f8c}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([mozProtocolHandler]);
