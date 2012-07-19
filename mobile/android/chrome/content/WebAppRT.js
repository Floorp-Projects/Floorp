/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

var WebAppRT = {
  init: function() {
    this.deck = document.getElementById("browsers");
    this.deck.addEventListener("click", this, false, true);
  },

  handleEvent: function(event) {
    let target = event.target;
  
    if (!(target instanceof HTMLAnchorElement) ||
        target.getAttribute("target") != "_blank") {
      return;
    }
  
    let uri = Services.io.newURI(target.href,
                                 target.ownerDocument.characterSet,
                                 null);
  
    // Direct the URL to the browser.
    Cc["@mozilla.org/uriloader/external-protocol-service;1"].
      getService(Ci.nsIExternalProtocolService).
      getProtocolHandlerInfo(uri.scheme).
      launchWithURI(uri);
  
    // Prevent the runtime from loading the URL.  We do this after directing it
    // to the browser to give the runtime a shot at handling the URL if we fail
    // to direct it to the browser for some reason.
    event.preventDefault();
  }
}
