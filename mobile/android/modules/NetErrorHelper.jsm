/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

this.EXPORTED_SYMBOLS = ["NetErrorHelper"];

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
