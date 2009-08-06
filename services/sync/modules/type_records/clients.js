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

const EXPORTED_SYMBOLS = ['ClientRecord'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/base_records/wbo.js");

function ClientRecord(uri) {
  this._ClientRec_init(uri);
}
ClientRecord.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.Client",

  _ClientRec_init: function ClientRec_init(uri) {
    this._WBORec_init(uri);
  },

  _escape: function ClientRecord__escape(toAscii) {
    // Escape-to-ascii or unescape-from-ascii each value
    if (this.payload != null)
      for (let [key, val] in Iterator(this.payload))
        this.payload[key] = (toAscii ? escape : unescape)(val);
  },

  serialize: function ClientRecord_serialize() {
    // Convert potential non-ascii to ascii before serializing
    this._escape(true);
    let ret = WBORecord.prototype.serialize.apply(this, arguments);

    // Restore the original data for normal use
    this._escape(false);
    return ret;
  },

  deserialize: function ClientRecord_deserialize(json) {
    // Deserialize then convert potential escaped non-ascii
    WBORecord.prototype.deserialize.apply(this, arguments);
    this._escape(false);
  },

  // engines.js uses cleartext to determine if records _isEqual
  // XXX Bug 482669 Implement .equals() for SyncEngine to compare records
  get cleartext() this.serialize(),

  // XXX engines.js calls encrypt/decrypt for all records, so define these:
  encrypt: function ClientRecord_encrypt(passphrase) {},
  decrypt: function ClientRecord_decrypt(passphrase) {}
};
