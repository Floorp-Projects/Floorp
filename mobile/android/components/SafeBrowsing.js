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
 * The Original Code is Safe Browsing.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kPhishWardenEnabledPref = "browser.safebrowsing.enabled";
const kMalwareWardenEnabledPref = "browser.safebrowsing.malware.enabled";

// This XPCOM object doesn't have a public interface. It just works quietly in the background
function SafeBrowsing() {
  this.listManager = null;

  // Once we register tables, their respective names will be listed here.
  this.phishing = {
    pref: kPhishWardenEnabledPref,
    blackTables: [],
    whiteTables: []
  };
  this.malware = {
    pref: kMalwareWardenEnabledPref,
    blackTables: [],
    whiteTables: []
  };

  // Get notifications when the phishing or malware warden enabled pref changes
  Services.prefs.addObserver(kPhishWardenEnabledPref, this, true);
  Services.prefs.addObserver(kMalwareWardenEnabledPref, this, true);
}

SafeBrowsing.prototype = {
  classID: Components.ID("{aadaed90-6c03-42d0-924a-fc61198ff283}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISessionStore,
                                         Ci.nsIDOMEventListener,
                                         Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function sb_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "app-startup":
        Services.obs.addObserver(this, "final-ui-startup", true);
        Services.obs.addObserver(this, "xpcom-shutdown", true);
        break;
      case "final-ui-startup":
        Services.obs.removeObserver(this, "final-ui-startup");
        this._startup();
        break;
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        this._shutdown();
        break;
      case "nsPref:changed":
        if (aData == kPhishWardenEnabledPref)
          this.maybeToggleUpdateChecking(this.phishing);
        else if (aData == kMalwareWardenEnabledPref)
          this.maybeToggleUpdateChecking(this.malware);
        break;
    }
  },

  _startup: function sb_startup() {
    this.listManager = Cc["@mozilla.org/url-classifier/listmanager;1"].getService(Ci.nsIUrlListManager);

    // Add a test chunk to the database
    let testData = "mozilla.org/firefox/its-an-attack.html";
    let testUpdate =
      "n:1000\ni:test-malware-simple\nad:1\n" +
      "a:1:32:" + testData.length + "\n" +
      testData;

    testData = "mozilla.org/firefox/its-a-trap.html";
    testUpdate +=
      "n:1000\ni:test-phish-simple\nad:1\n" +
      "a:1:32:" + testData.length + "\n" +
      testData;

    let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(Ci.nsIUrlClassifierDBService);

    let listener = {
      QueryInterface: function(aIID) {
        if (aIID.equals(Ci.nsISupports) || aIID.equals(Ci.nsIUrlClassifierUpdateObserver))
          return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
      },

      updateUrlRequested: function(aURL) { },
      streamFinished: function(aStatus) { },
      updateError: function(aErrorCode) { },
      updateSuccess: function(aRequestedTimeout) { }
    };

    try {
      dbService.beginUpdate(listener, "test-malware-simple,test-phish-simple", "");
      dbService.beginStream("", "");
      dbService.updateStream(testUpdate);
      dbService.finishStream();
      dbService.finishUpdate();
    } catch(ex) {}

    this.registerBlackTable("goog-malware-shavar", this.malware);
    this.maybeToggleUpdateChecking(this.malware);

    this.registerBlackTable("goog-phish-shavar", this.phishing);
    this.maybeToggleUpdateChecking(this.phishing);
  },

  _shutdown: function sb_shutdown() {
    Services.prefs.removeObserver(kPhishWardenEnabledPref, this);
    Services.prefs.removeObserver(kMalwareWardenEnabledPref, this);

    this.listManager = null;
  },

  enableBlacklistTableUpdates: function sb_enableBlacklistTableUpdates(aWarden) {
    for (let i = 0; i < aWarden.blackTables.length; ++i) {
      this.listManager.enableUpdate(aWarden.blackTables[i]);
    }
  },

  disableBlacklistTableUpdates: function sb_disableBlacklistTableUpdates(aWarden) {
    for (let i = 0; i < aWarden.blackTables.length; ++i) {
      this.listManager.disableUpdate(aWarden.blackTables[i]);
    }
  },

  enableWhitelistTableUpdates: function sb_enableWhitelistTableUpdates(aWarden) {
    for (let i = 0; i < this.whiteTables.length; ++i) {
      this.listManager.enableUpdate(this.whiteTables[i]);
    }
  },

  disableWhitelistTableUpdates: function sb_disableWhitelistTableUpdates(aWarden) {
    for (let i = 0; i < aWarden.whiteTables.length; ++i) {
      this.listManager.disableUpdate(aWarden.whiteTables[i]);
    }
  },

  registerBlackTable: function sb_registerBlackTable(aTableName, aWarden) {
    let result = this.listManager.registerTable(aTableName, false);
    if (result)
      aWarden.blackTables.push(aTableName);
    return result;
  },

  registerWhiteTable: function sb_registerWhiteTable(aTableName, aWarden) {
    let result = this.listManager.registerTable(aTableName, false);
    if (result)
      aWarden.whiteTables.push(aTableName);
    return result;
  },

  maybeToggleUpdateChecking: function sb_maybeToggleUpdateChecking(aWarden) {
    let enabled = Services.prefs.getBoolPref(aWarden.pref);
    if (enabled)
      this.enableBlacklistTableUpdates(aWarden);
    else
      this.disableBlacklistTableUpdates(aWarden);
  }
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([SafeBrowsing]);
