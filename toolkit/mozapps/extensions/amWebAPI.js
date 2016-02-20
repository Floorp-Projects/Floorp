/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

function WebAPI() {
}

WebAPI.prototype = {
  init(window) {
    this.window = window;
  },

  getAddonByID(id) {
    return this.window.Promise.reject("Not yet implemented");
  },

  classID: Components.ID("{8866d8e3-4ea5-48b7-a891-13ba0ac15235}"),
  contractID: "@mozilla.org/addon-web-api/manager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIDOMGlobalPropertyInitializer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WebAPI]);
