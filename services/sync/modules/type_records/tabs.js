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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jono DiCarlo <jdicarlo@mozilla.com>
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

const EXPORTED_SYMBOLS = ['TabSetRecord'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");

function TabSetRecord(uri) {
  this._TabSetRecord_init(uri);
}
TabSetRecord.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Record.Tabs",

  _TabSetRecord_init: function TabSetRecord__init(uri) {
    this._CryptoWrap_init(uri);
    this.cleartext = {
    };
  },

  addTab: function TabSetRecord_addTab(title, urlHistory, lastUsed) {
    if (!this.cleartext.tabs)
      this.cleartext.tabs = [];
    if (!title) {
      title = "";
    }
    if (!lastUsed) {
      lastUsed = 0;
    }
    if (!urlHistory || urlHistory.length == 0) {
      return;
    }
    this.cleartext.tabs.push( {title: title,
                               urlHistory: urlHistory,
                               lastUsed: lastUsed});
  },

  getAllTabs: function TabSetRecord_getAllTabs() {
    return this.cleartext.tabs;
  },

  setClientName: function TabSetRecord_setClientName(value) {
    this.cleartext.clientName = value;
  },

  getClientName: function TabSetRecord_getClientName() {
    return this.cleartext.clientName;
  },

  toJson: function TabSetRecord_toJson() {
    return this.cleartext;
  },

  fromJson: function TabSetRecord_fromJson(json) {
    this.cleartext = json;
  }
};
