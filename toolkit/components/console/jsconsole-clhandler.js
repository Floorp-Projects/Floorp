# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# vim:sw=4:sr:sta:et:sts:
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Martijn Pieters <mj@digicool.com>
#   Benjamin Smedberg <benjamin@smedbergs.us>
#   Simon Bünzli <zeniko@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
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

  helpInfo : "  -jsconsole           Open the Error console.\n",

  classDescription: "jsConsoleHandler",
  classID: Components.ID("{2cd0c310-e127-44d0-88fc-4435c9ab4d4b}"),
  contractID: "@mozilla.org/toolkit/console-clh;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
  _xpcom_categories: [{category: "command-line-handler", entry: "b-jsconsole"}]
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([jsConsoleHandler]);
