/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function CommandLineHandler() {}

CommandLineHandler.prototype = {
  classID: Components.ID("{6d69c782-40a3-469b-8bfd-3ee366105a4a}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  handle: function handle(cmdLine) {
    Services.ww.openWindow(null,
                           "chrome://webapprt/content/webapp.xul",
                           "_blank",
                           "chrome,dialog=no,all,resizable",
                           null);
  },

  helpInfo : "",
};

let components = [CommandLineHandler];
let NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
