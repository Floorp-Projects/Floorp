/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/UITelemetry.jsm");

this.EXPORTED_SYMBOLS = ["NetErrorHelper"];

const KEY_CODE_ENTER = 13;

/* Handlers is a list of objects that will be notified when an error page is shown
 * or when an event occurs on the page that they are registered to handle. Registration
 * is done by just adding yourself to the dictionary.
 *
 * handlers.myKey = {
 *   onPageShown: function(browser) { },
 *   handleEvent: function(event) { },
 * }
 *
 * The key that you register yourself with should match the ID of the element you want to
 * watch for click events on.
 */

let handlers = {};

function NetErrorHelper(browser) {
  browser.addEventListener("click", this.handleClick, true);

  let listener = () => {
    browser.removeEventListener("click", this.handleClick, true);
    browser.removeEventListener("pagehide", listener, true);
  };
  browser.addEventListener("pagehide", listener, true);

  // Handlers may want to customize the page
  for (let id in handlers) {
    if (handlers[id].onPageShown) {
      handlers[id].onPageShown(browser);
    }
  }
}

NetErrorHelper.attachToBrowser = function(browser) {
  return new NetErrorHelper(browser);
}

NetErrorHelper.prototype = {
  handleClick: function(event) {
    let node = event.target;

    while(node) {
      if (node.id in handlers && handlers[node.id].handleClick) {
        handlers[node.id].handleClick(event);
        return;
      }

      node = node.parentNode;
    }
  },
}

handlers.searchbutton = {
  onPageShown: function(browser) {
    let search = browser.contentDocument.querySelector("#searchbox");
    if (!search) {
      return;
    }

    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    let tab = browserWin.BrowserApp.getTabForBrowser(browser);

    // If there is no stored userRequested, just hide the searchbox
    if (!tab.userRequested) {
      search.style.display = "none";
    } else {
      let text = browser.contentDocument.querySelector("#searchtext");
      text.value = tab.userRequested;
      text.addEventListener("keypress", (event) => {
        if (event.keyCode === KEY_CODE_ENTER) {
          this.doSearch(event.target.value);
        }
      });
    }
  },

  handleClick: function(event) {
    let value = event.target.previousElementSibling.value;
    this.doSearch(value);
  },

  doSearch: function(value) {
    let engine = Services.search.defaultEngine;
    let uri = engine.getSubmission(value).uri;

    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    // Reset the user search to whatever the new search term was
    browserWin.BrowserApp.loadURI(uri.spec, undefined, { isSearch: true, userRequested: value });
  }
};

handlers.wifi = {
  // This registers itself with the nsIObserverService as a weak ref,
  // so we have to implement GetWeakReference as well.
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  GetWeakReference: function() {
    return Cu.getWeakReference(this);
  },

  onPageShown: function(browser) {
      // If we have a connection, don't bother showing the wifi toggle.
      let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
      if (network.isLinkUp && network.linkStatusKnown) {
        let nodes = browser.contentDocument.querySelectorAll("#wifi");
        for (let i = 0; i < nodes.length; i++) {
          nodes[i].style.display = "none";
        }
      }
  },

  handleClick: function(event) {
    let node = event.target;
    while(node && node.id !== "wifi") {
      node = node.parentNode;
    }

    if (!node) {
      return;
    }

    UITelemetry.addEvent("neterror.1", "button", "wifitoggle");
    // Show indeterminate progress while we wait for the network.
    node.disabled = true;
    node.classList.add("inProgress");

    this.node = Cu.getWeakReference(node);
    Services.obs.addObserver(this, "network:link-status-changed", true);

    Messaging.sendRequest({
      type: "Wifi:Enable"
    });
  },

  observe: function(subject, topic, data) {
    let node = this.node.get();
    if (!node) {
      return;
    }

    // Remove the progress bar
    node.disabled = false;
    node.classList.remove("inProgress");

    let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
    if (network.isLinkUp && network.linkStatusKnown) {
      // If everything worked, reload the page
      UITelemetry.addEvent("neterror.1", "button", "wifitoggle.reload");
      Services.obs.removeObserver(this, "network:link-status-changed");

      // Even at this point, Android sometimes lies about the real state of the network and this reload request fails.
      // Add a 500ms delay before refreshing the page.
      node.ownerDocument.defaultView.setTimeout(function() {
        node.ownerDocument.location.reload(false);
      }, 500);
    }
  }
}

