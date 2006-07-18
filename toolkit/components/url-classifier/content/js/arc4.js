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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marius Schilder <mschilder@google.com> (original author)
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


/**
 * ARC4 streamcipher implementation
 * @constructor
 */
function ARC4() {
  this._S = new Array(256);
  this._i = 0;
  this._j = 0;
}

/**
 * Initialize the cipher for use with new key.
 * @param {byte[]} key is byte array containing key
 * @param {int} opt_length indicates # of bytes to take from key
 */
ARC4.prototype.setKey = function(key, opt_length) {
  if (!isArray(key)) {
    throw new Error("Key parameter must be a byte array");
  }

  if (!opt_length) {
    opt_length = key.length;
  }

  var S = this._S;

  for (var i = 0; i < 256; ++i) {
    S[i] = i;
  }

  var j = 0;
  for (var i = 0; i < 256; ++i) {
    j = (j + S[i] + key[i % opt_length]) & 255;

    var tmp = S[i];
    S[i] = S[j];
    S[j] = tmp;
  }
    
  this._i = 0;
  this._j = 0;
}

/**
 * Discard n bytes of the keystream.
 * These days 1536 is considered a decent amount to drop to get
 * the key state warmed-up enough for secure usage.
 * This is not done in the constructor to preserve efficiency for
 * use cases that do not need this.
 * @param {int} n is # of bytes to disregard from stream
 */
ARC4.prototype.discard = function(n) {
  var devnul = new Array(n);
  this.crypt(devnul);
}

/**
 * En- or decrypt (same operation for streamciphers like ARC4)
 * @param {byte[]} data gets xor-ed in place
 * @param {int} opt_length indicated # of bytes to crypt
 */
ARC4.prototype.crypt = function(data, opt_length) {
  if (!opt_length) {
    opt_length = data.length;
  }

  var i = this._i;
  var j = this._j;
  var S = this._S;

  for (var n = 0; n < opt_length; ++n) {
    i = (i + 1) & 255;
    j = (j + S[i]) & 255;

    var tmp = S[i];
    S[i] = S[j];
    S[j] = tmp;

    data[n] ^= S[(S[i] + S[j]) & 255];
  }

  this._i = i;
  this._j = j;
}
