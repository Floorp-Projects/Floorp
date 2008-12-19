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
 *  Chris Beard <chris@mozilla.com>
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

const EXPORTED_SYMBOLS = ['ClientData'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'ClientData', ClientDataSvc);

function ClientDataSvc() {
  this._init();
}
ClientDataSvc.prototype = {
  get GUID() {
    return this._getCharPref("client.GUID", function() Utils.makeGUID());
  },
  set GUID(value) {
    Utils.prefs.setCharPref("client.GUID", value);
  },

  get name() {
    return this._getCharPref("client.name", function() "Firefox");
  },
  set GUID(value) {
    Utils.prefs.setCharPref("client.name", value);
  },

  get type() {
    return this._getCharPref("client.type", function() "desktop");
  },
  set GUID(value) {
    Utils.prefs.setCharPref("client.type", value);
  },

  clients: function ClientData__clients() {
    return this._remote.data;
  },

  _getCharPref: function ClientData__getCharPref(pref, defaultCb) {
    let value;
    try {
      value = Utils.prefs.getCharPref(pref);
    } catch (e) {
      value = defaultCb();
      Utils.prefs.setCharPref(pref, value);
    }
    return value;
  },

  _init: function ClientData__init() {
    this._log = Log4Moz.repository.getLogger("Service.ClientData");
    this._remote = new Resource("meta/clients");
    this._remote.pushFilter(new JsonFilter());
  },

  _wrap: function ClientData__wrap() {
    return {
      GUID: this.GUID,
      name: this.name,
      type: this.type
    };
  },

  _refresh: function ClientData__refresh() {
    let self = yield;

   // No more such thing as DAV.  I think this is making a directory.
    // Will the directory now be handled automatically? (YES)
   /* let ret = yield DAV.MKCOL("meta", self.cb);
    if(!ret)
      throw "Could not create meta information directory";*/

    // This fails horribly because this._remote._uri.spec is null. What's
    // that mean?
    // Spec is supposed to be just a string?

    // Probably problem has to do with Resource getting intialized by
    // relative, not absolute, path?
    // Use WBORecord (Weave Basic Object) from wbo.js?
    this._log.debug("The URI is " + this._remote._uri);
    this._log.debug("The URI.spec is " + this._remote._uri.spec);
    try { yield this._remote.get(self.cb); }
    catch (e if e.status == 404) {
      this._log.debug("404ed.  Using empty for remote data.");
      this._remote.data = {};
    }

    this._remote.data[this.GUID] = this._wrap();
    yield this._remote.put(self.cb);
    this._log.debug("Successfully downloaded clients file from server");
  },
  refresh: function ClientData_refresh(onComplete) {
    this._refresh.async(this, onComplete);
  }
};
