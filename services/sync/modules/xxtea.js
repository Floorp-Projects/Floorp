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
 * The Original Code is Corrected Block TEA.
 *
 * The Initial Developer of the Original Code is
 * Chris Veness
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Veness <chrisv@movable-type.co.uk>
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

// Original 'Corrected Block TEA' algorithm David Wheeler & Roger Needham
// See http://en.wikipedia.org/wiki/XXTEA
//
// Javascript version by Chris Veness
// http://www.movable-type.co.uk/scripts/tea.html

const EXPORTED_SYMBOLS = ['encrypt', 'decrypt'];

function Paused() {
}
Paused.prototype = {
  toString: function Paused_toString() {
    return "[Generator Paused]";
  }
}

// use (16 chars of) 'password' to encrypt 'plaintext'
//
// note1:this is a generator so the caller can pause and give control
// to the UI thread
//
// note2: if plaintext or password are passed as string objects, rather
// than strings, this function will throw an 'Object doesn't support
// this property or method' error

function encrypt(plaintext, password) {
  var v = new Array(2), k = new Array(4), s = "", i;

  // use escape() so only have single-byte chars to encode 
  plaintext = escape(plaintext);

  // build key directly from 1st 16 chars of password
  for (i = 0; i < 4; i++)
    k[i] = Str4ToLong(password.slice(i * 4, (i + 1) * 4));

  for (i = 0; i < plaintext.length; i += 8) {
    // encode plaintext into s in 64-bit (8 char) blocks
    // ... note this is 'electronic codebook' mode
    v[0] = Str4ToLong(plaintext.slice(i, i + 4));
    v[1] = Str4ToLong(plaintext.slice(i + 4, i + 8));
    code(v, k);
    s += LongToStr4(v[0]) + LongToStr4(v[1]);

    if (i % 512 == 0)
      yield new Paused();
  }

  yield escCtrlCh(s);
}

// use (16 chars of) 'password' to decrypt 'ciphertext' with xTEA

function decrypt(ciphertext, password) {
  var v = new Array(2), k = new Array(4), s = "", i;

  for (i = 0; i < 4; i++)
    k[i] = Str4ToLong(password.slice(i * 4, (i + 1) * 4));

  ciphertext = unescCtrlCh(ciphertext);
  for (i = 0; i < ciphertext.length; i += 8) {
    // decode ciphertext into s in 64-bit (8 char) blocks
    v[0] = Str4ToLong(ciphertext.slice(i, i + 4));
    v[1] = Str4ToLong(ciphertext.slice(i + 4, i + 8));
    decode(v, k);
    s += LongToStr4(v[0]) + LongToStr4(v[1]);

    if (i % 512 == 0)
      yield new Paused();
  }

  // strip trailing null chars resulting from filling 4-char blocks:
  s = s.replace(/\0+$/, '');

  yield unescape(s);
}


function code(v, k) {
  // Extended TEA: this is the 1997 revised version of Needham & Wheeler's algorithm
  // params: v[2] 64-bit value block; k[4] 128-bit key
  var y = v[0], z = v[1];
  var delta = 0x9E3779B9, limit = delta*32, sum = 0;

  while (sum != limit) {
    y += (z<<4 ^ z>>>5)+z ^ sum+k[sum & 3];
    sum += delta;
    z += (y<<4 ^ y>>>5)+y ^ sum+k[sum>>>11 & 3];
    // note: unsigned right-shift '>>>' is used in place of original '>>', due to lack 
    // of 'unsigned' type declaration in JavaScript (thanks to Karsten Kraus for this)
  }
  v[0] = y; v[1] = z;
}

function decode(v, k) {
  var y = v[0], z = v[1];
  var delta = 0x9E3779B9, sum = delta*32;

  while (sum != 0) {
    z -= (y<<4 ^ y>>>5)+y ^ sum+k[sum>>>11 & 3];
    sum -= delta;
    y -= (z<<4 ^ z>>>5)+z ^ sum+k[sum & 3];
  }
  v[0] = y; v[1] = z;
}


// supporting functions

function Str4ToLong(s) {  // convert 4 chars of s to a numeric long
  var v = 0;
  for (var i=0; i<4; i++) v |= s.charCodeAt(i) << i*8;
  return isNaN(v) ? 0 : v;
}

function LongToStr4(v) {  // convert a numeric long to 4 char string
  var s = String.fromCharCode(v & 0xFF, v>>8 & 0xFF, v>>16 & 0xFF, v>>24 & 0xFF);
  return s;
}

function escCtrlCh(str) {  // escape control chars which might cause problems with encrypted texts
  return str.replace(/[\0\t\n\v\f\r\xa0'"!]/g, function(c) { return '!' + c.charCodeAt(0) + '!'; });
}

function unescCtrlCh(str) {  // unescape potentially problematic nulls and control characters
  return str.replace(/!\d\d?\d?!/g, function(c) { return String.fromCharCode(c.slice(1,-1)); });
}
