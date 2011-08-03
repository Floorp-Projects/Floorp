/*
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
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
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
*/

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const XPI_CONTENT_TYPE = "application/x-xpinstall";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function amContentHandler() {
}

amContentHandler.prototype = {
  /**
   * Handles a new request for an application/x-xpinstall file.
   *
   * @param  aMimetype
   *         The mimetype of the file
   * @param  aContext
   *         The context passed to nsIChannel.asyncOpen
   * @param  aRequest
   *         The nsIRequest dealing with the content
   */
  handleContent: function XCH_handleContent(aMimetype, aContext, aRequest) {
    if (aMimetype != XPI_CONTENT_TYPE)
      throw Cr.NS_ERROR_WONT_HANDLE_CONTENT;

    if (!(aRequest instanceof Ci.nsIChannel))
      throw Cr.NS_ERROR_WONT_HANDLE_CONTENT;

    let uri = aRequest.URI;

    let window = null;
    let callbacks = aRequest.notificationCallbacks ?
                    aRequest.notificationCallbacks :
                    aRequest.loadGroup.notificationCallbacks;
    if (callbacks)
      window = callbacks.getInterface(Ci.nsIDOMWindow);

    aRequest.cancel(Cr.NS_BINDING_ABORTED);

    let appinfo = Cc["@mozilla.org/xre/app-info;1"].
                  getService(Ci.nsIXULRuntime);
    if (appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
      try {
        if (!window.InstallTrigger) 
          window = window.wrappedJSObject;
  
        if (window.InstallTrigger)
          window.InstallTrigger.startSoftwareUpdate(uri.spec);
        else
          this.log("Window does not have an InstallTrigger");
      } catch(ex) {
        this.log(ex);
      }
    }
    else {
      let referer = null;
      if (aRequest instanceof Ci.nsIPropertyBag2) {
        referer = aRequest.getPropertyAsInterface("docshell.internalReferrer",
                                                Ci.nsIURI);
      }

      let manager = Cc["@mozilla.org/addons/integration;1"].
                    getService(Ci.amIWebInstaller);
      manager.installAddonsFromWebpage(aMimetype, window, referer, [uri.spec],
                                       [null], [null], [null], null, 1);
    }
  },

  classID: Components.ID("{7beb3ba8-6ec3-41b4-b67c-da89b8518922}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentHandler]),

  log : function(aMsg) {
    let msg = "amContentHandler.js: " + (aMsg.join ? aMsg.join("") : aMsg);
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
      logStringMessage(msg);
    dump(msg + "\n");
  }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([amContentHandler]);
