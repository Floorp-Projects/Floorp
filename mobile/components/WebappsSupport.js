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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>
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

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DB_VERSION = 1;

function WebappsSupport() {
  this.init();
}

WebappsSupport.prototype = {
  db: null,
  
  init: function() {
    let file =  Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
    file.append("webapps.sqlite");
    this.db = Services.storage.openDatabase(file);
    let version = this.db.schemaVersion;
    
    if (version == 0) {
      this.db.executeSimpleSQL("CREATE TABLE webapps (title TEXT, uri TEXT PRIMARY KEY, icon TEXT)");
      this.db.schemaVersion = DB_VERSION;
    }

    XPCOMUtils.defineLazyGetter(this, "_installQuery", function() {
      return this.db.createAsyncStatement("INSERT INTO webapps (title, uri, icon) VALUES(:title, :uri, :icon)");
    });

    XPCOMUtils.defineLazyGetter(this, "_findQuery", function() {
      return this.db.createStatement("SELECT uri FROM webapps where uri = :uri");
    });

    Services.obs.addObserver(this, "quit-application-granted", false);
  },
 
  // entry point
  installApplication: function(aTitle, aURI, aIconURI, aIconData) {
    let stmt = this._installQuery;
    stmt.params.title = aTitle;
    stmt.params.uri = aURI;
    stmt.params.icon = aIconData;
    stmt.executeAsync();
  },
 
  isApplicationInstalled: function(aURI) {
    let stmt = this._findQuery;
    let found = false;
    try {
      stmt.params.uri = aURI;
      found = stmt.executeStep();
    } finally {
      stmt.reset();
    }
    return found;
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(this, "quit-application-granted");

    // Finalize the statements that we have used
    let stmts = [
      "_installQuery",
      "_findQuery"
    ];
    for (let i = 0; i < stmts.length; i++) {
      // We do not want to create any query we haven't already created, so
      // see if it is a getter first.
      if (Object.getOwnPropertyDescriptor(this, stmts[i]).value !== undefined) {
        this[stmts[i]].finalize();
      }
    }

    this.db.asyncClose();
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebappsSupport]),

  // XPCOMUtils factory
  classID: Components.ID("{cb1107c1-1e15-4f11-99c8-27b9ec221a2a}")
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WebappsSupport]);

