/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:sw=4:sr:sta:et:sts: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function jsConsoleHandler() {}
jsConsoleHandler.prototype = {
  handle: function clh_handle(cmdLine) {
    if (!cmdLine.handleFlag("jsconsole", false))
      return;

    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    var console = wm.getMostRecentWindow("global:console");
    if (!console) {
      var wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
      wwatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                        "chrome,dialog=no,all", cmdLine);
    } else {
      console.focus(); // the Error console was already open
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_REMOTE_AUTO)
      cmdLine.preventDefault = true;
  },

  helpInfo : "  -jsconsole         Open the Error console.\n",

  classID: Components.ID("{2cd0c310-e127-44d0-88fc-4435c9ab4d4b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([jsConsoleHandler]);
