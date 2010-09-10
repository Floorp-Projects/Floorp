/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function WeaveService() {
  this.wrappedJSObject = this;
}
WeaveService.prototype = {
  classID: Components.ID("{74b89fb0-f200-4ae8-a3ec-dd164117f6de}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function BSS__observe(subject, topic, data) {
    switch (topic) {
    case "app-startup":
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.addObserver(this, "final-ui-startup", true);
      break;

    case "final-ui-startup":
      this.addResourceAlias();

      // Force Weave service to load if it hasn't triggered from overlays
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.timer.initWithCallback({
        notify: function() {
          Cu.import("resource://services-sync/main.js");
          if (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED)
            Weave.Service;
        }
      }, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
      break;
    }
  },

  addResourceAlias: function() {
    let ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
    let resProt = ioService.getProtocolHandler("resource")
                  .QueryInterface(Ci.nsIResProtocolHandler);

    // Only create alias if resource://services-sync doesn't already exist.
    if (resProt.hasSubstitution("services-sync"))
      return;

    let uri = ioService.newURI("resource://gre/modules/services-sync/",
                               null, null);
    resProt.setSubstitution("services-sync", uri);
  }
};

function AboutWeaveLog() {}
AboutWeaveLog.prototype = {
  classID: Components.ID("{d28f8a0b-95da-48f4-b712-caf37097be41}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule,
                                         Ci.nsISupportsWeakReference]),

  getURIFlags: function(aURI) {
    return 0;
  },

  newChannel: function(aURI) {
    let dir = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);
    let file = dir.get("ProfD", Ci.nsILocalFile);
    file.append("weave");
    file.append("logs");
    file.append("verbose-log.txt");
    let ios = Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
    let ch = ios.newChannel(ios.newFileURI(file).spec, null, null);
    ch.originalURI = aURI;
    return ch;
  }
};

function AboutWeaveLog1() {}
AboutWeaveLog1.prototype = {
  classID: Components.ID("{a08ee179-df50-48e0-9c87-79e4dd5caeb1}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule,
                                         Ci.nsISupportsWeakReference]),

  getURIFlags: function(aURI) {
    return 0;
  },

  newChannel: function(aURI) {
    let dir = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);
    let file = dir.get("ProfD", Ci.nsILocalFile);
    file.append("weave");
    file.append("logs");
    file.append("verbose-log.txt.1");
    let ios = Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
    let ch = ios.newChannel(ios.newFileURI(file).spec, null, null);
    ch.originalURI = aURI;
    return ch;
  }
};

let components = [WeaveService, AboutWeaveLog, AboutWeaveLog1];

// Gecko <2.0
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}

// Gecko >=2.0
if (typeof XPCOMUtils.generateNSGetFactory == "function")
    const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
