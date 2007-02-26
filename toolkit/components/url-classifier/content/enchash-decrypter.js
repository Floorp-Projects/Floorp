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
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Fritz Schneider <fritz@google.com> (original author)
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


// This is the code used to interact with data encoded in the
// goog-black-enchash format. The format is basically a map from
// hashed hostnames to encrypted sequences of regular expressions
// where the encryption key is derived from the hashed
// hostname. Encoding lists like this raises the bar slightly on
// deriving complete table data from the db. This data format is NOT
// our idea; we would've raise the bar higher :)
//
// Anyway, this code is a port of the original C++ implementation by
// Garret. To ease verification, I mirrored that code as closely as
// possible.  As a result, you'll see some C++-style variable naming
// and roundabout (C++) ways of doing things. Additionally, I've
// omitted the comments.
//
// This code should not change, except to fix bugs.
//
// TODO: accommodate other kinds of perl-but-not-javascript qualifiers

/**
 * A fast, bit-vector map for ascii characters.
 *
 * Internally stores 256 bits in an array of 8 ints.
 * Does quick bit-flicking to lookup needed characters.
 */

/**
 * @param Takes 8 ints to initialize the character map
 */
function Charmap() {
  if (arguments.length != 8) {
    throw G_Error("charmap ctor requires 8 int args");
  }
  this.map_ = [];
  for (var i = 0; i < 8; ++i) {
    this.map_.push(arguments[i]);
  }
}

/**
 * Do a quick lookup to see if the letter is in the map.
 * @param chr String of length 1 (ascii)
 * @return Boolean true if the letter is in the map
 */
Charmap.prototype.contains = function(chr) {
  var val = chr.charCodeAt(0);
  return !!(this.map_[val >> 5] & (1 << (val & 31)));
}

/**
 * This thing knows how to generate lookup keys and decrypt values found in
 * a table of type enchash.
 */
function PROT_EnchashDecrypter() {
  this.debugZone = "enchashdecrypter";
  this.REs_ = PROT_EnchashDecrypter.REs;
  this.hasher_ = new G_CryptoHasher();
  this.base64_ = new G_Base64();
  this.streamCipher_ = Cc["@mozilla.org/security/streamcipher;1"]
                       .createInstance(Ci.nsIStreamCipher);
  this.escapeCharmap_ = new Charmap(
    0xffffffff, 0xfc009fff, 0xf8000001, 0xf8000001,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
}

PROT_EnchashDecrypter.DATABASE_SALT = "oU3q.72p";
PROT_EnchashDecrypter.SALT_LENGTH = PROT_EnchashDecrypter.DATABASE_SALT.length;

PROT_EnchashDecrypter.MAX_DOTS = 5;

PROT_EnchashDecrypter.REs = {};
PROT_EnchashDecrypter.REs.FIND_DODGY_CHARS_GLOBAL =
  new RegExp("[\x00-\x1f\x7f-\xff]+", "g");
PROT_EnchashDecrypter.REs.FIND_END_DOTS_GLOBAL =
  new RegExp("^\\.+|\\.+$", "g");
PROT_EnchashDecrypter.REs.FIND_MULTIPLE_DOTS_GLOBAL =
  new RegExp("\\.{2,}", "g");
PROT_EnchashDecrypter.REs.FIND_TRAILING_SPACE =
  new RegExp("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}) ");
PROT_EnchashDecrypter.REs.POSSIBLE_IP =
  new RegExp("^((?:0x[0-9a-f]+|[0-9\\.])+)$", "i");
PROT_EnchashDecrypter.REs.FIND_BAD_OCTAL = new RegExp("(^|\\.)0\\d*[89]");
PROT_EnchashDecrypter.REs.IS_OCTAL = new RegExp("^0[0-7]*$");
PROT_EnchashDecrypter.REs.IS_DECIMAL = new RegExp("^[0-9]+$");
PROT_EnchashDecrypter.REs.IS_HEX = new RegExp("^0[xX]([0-9a-fA-F]+)$");

// Regexps are given in perl regexp format. Unfortunately, JavaScript's
// library isn't completely compatible. For example, you can't specify
// case-insensitive matching by using (?i) in the expression text :(
// So we manually set this bit with the help of this regular expression.
PROT_EnchashDecrypter.REs.CASE_INSENSITIVE = /\(\?i\)/g;

/**
 * Helper function 
 *
 * @param str String to get chars from
 * 
 * @param n Number of characters to get
 *
 * @returns String made up of the last n characters of str
 */ 
PROT_EnchashDecrypter.prototype.lastNChars_ = function(str, n) {
  n = -n;
  return str.substr(n);
}

/**
 * We have to have our own hex-decoder because decodeURIComponent
 * expects UTF-8 (so it will barf on invalid UTF-8 sequences).
 *
 * @param str String to decode
 * 
 * @returns The decoded string
 */
PROT_EnchashDecrypter.prototype.hexDecode_ = function(str) {
  var output = [];

  var i = 0;
  while (i < str.length) {
    var c = str.charAt(i);
  
    if (c == "%" && i + 2 < str.length) {

      var asciiVal = Number("0x" + str.charAt(i + 1) + str.charAt(i + 2));
      
      if (!isNaN(asciiVal)) {
        i += 2;
        c = String.fromCharCode(asciiVal);
      }
    }
    
    output[output.length] = c;
    ++i;
  }
  
  return output.join("");
}

/**
 * Translate a plaintext enchash value into regular expressions
 *
 * @param data String containing a decrypted enchash db entry
 *
 * @returns An array of RegExps
 */
PROT_EnchashDecrypter.prototype.parseRegExps = function(data) {
  var res = data.split("\t");
  
  G_Debug(this, "Got " + res.length + " regular rexpressions");
  
  for (var i = 0; i < res.length; i++) {
    // Could have leading (?i); if so, set the flag and strip it
    var flags = (this.REs_.CASE_INSENSITIVE.test(res[i])) ? "i" : "";
    res[i] = res[i].replace(this.REs_.CASE_INSENSITIVE, "");
    res[i] = new RegExp(res[i], flags);
  }

  return res;
}

/**
 * Get the canonical version of the given URL for lookup in a table of 
 * type -url.
 *
 * @param url String to canonicalize
 *
 * @returns String containing the canonicalized url (maximally url-decoded
 *          with hostname normalized, then specially url-encoded)
 */
PROT_EnchashDecrypter.prototype.getCanonicalUrl = function(url) {
  var escapedUrl = PROT_URLCanonicalizer.canonicalizeURL_(url);
  // Normalize the host
  var host = this.getCanonicalHost(escapedUrl);
  if (!host) {
    // Probably an invalid url, return what we have so far.
    return escapedUrl;
  }

  // Combine our normalized host with our escaped url.
  var ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  var urlObj = ioService.newURI(escapedUrl, null, null);
  urlObj.host = host;
  return urlObj.asciiSpec;
}

/**
 * @param opt_maxDots Number maximum number of dots to include.
 */
PROT_EnchashDecrypter.prototype.getCanonicalHost = function(str, opt_maxDots) {
  var ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  try {
    var urlObj = ioService.newURI(str, null, null);
    var asciiHost = urlObj.asciiHost;
  } catch (e) {
    G_Debug(this, "Unable to get hostname: " + str);
    return "";
  }

  var unescaped = this.hexDecode_(asciiHost);

  unescaped = unescaped.replace(this.REs_.FIND_DODGY_CHARS_GLOBAL, "")
              .replace(this.REs_.FIND_END_DOTS_GLOBAL, "")
              .replace(this.REs_.FIND_MULTIPLE_DOTS_GLOBAL, ".");

  var temp = this.parseIPAddress_(unescaped);
  if (temp)
    unescaped = temp;

  // Escape everything that's not alphanumeric, hyphen, or dot.
  var escaped = '';
  for (var i = 0; i < unescaped.length; ++i) {
    if (this.escapeCharmap_.contains(unescaped[i])) {
      escaped += '%' + unescaped.charCodeAt(i).toString(16);
    } else {
      escaped += unescaped[i];
    }
  }

  if (opt_maxDots) {
    // Limit the number of dots
    var k;
    var index = escaped.length;
    for (k = 0; k < opt_maxDots + 1; k++) {
      temp = escaped.lastIndexOf(".", index - 1);
      if (temp == -1) {
        break;
      } else {
        index = temp;
      }
    }
    
    if (k == opt_maxDots + 1 && index != -1) {
      escaped = escaped.substring(index + 1);
    }
  }

  escaped = escaped.toLowerCase();
  return escaped;
}

PROT_EnchashDecrypter.prototype.parseIPAddress_ = function(host) {
  if (host.length <= 15) {

    // The Windows resolver allows a 4-part dotted decimal IP address to
    // have a space followed by any old rubbish, so long as the total length
    // of the string doesn't get above 15 characters. So, "10.192.95.89 xy"
    // is resolved to 10.192.95.89.
    // If the string length is greater than 15 characters, e.g.
    // "10.192.95.89 xy.wildcard.example.com", it will be resolved through
    // DNS.
    var match = this.REs_.FIND_TRAILING_SPACE.exec(host);
    if (match) {
      host = match[1];
    }
  }
  
  if (!this.REs_.POSSIBLE_IP.test(host))
    return "";

  var parts = host.split(".");
  if (parts.length > 4)
    return "";

  var allowOctal = !this.REs_.FIND_BAD_OCTAL.test(host);

  for (var k = 0; k < parts.length; k++) {
    var canon;
    if (k == parts.length - 1) {
      canon = this.canonicalNum_(parts[k], 5 - parts.length, allowOctal);
    } else {
      canon = this.canonicalNum_(parts[k], 1, allowOctal);
    }
    if (canon != "") 
      parts[k] = canon;
    else
      return "";
  }

  return parts.join(".");
}

PROT_EnchashDecrypter.prototype.canonicalNum_ = function(num, bytes, octal) {
  if (bytes < 0) 
    return "";
  var temp_num;

  if (octal && this.REs_.IS_OCTAL.test(num)) {

    num = this.lastNChars_(num, 11);

    temp_num = parseInt(num, 8);
    if (isNaN(temp_num))
      temp_num = -1;

  } else if (this.REs_.IS_DECIMAL.test(num)) {

    num = this.lastNChars_(num, 32);

    temp_num = parseInt(num, 10);
    if (isNaN(temp_num))
      temp_num = -1;

  } else if (this.REs_.IS_HEX.test(num)) {
    var matches = this.REs_.IS_HEX.exec(num);
    if (matches) {
      num = matches[1];
    }

    temp_num = parseInt(num, 16);
    if (isNaN(temp_num))
      temp_num = -1;

  } else {
    return "";
  }

  if (temp_num == -1) 
    return "";

  // Since we mod the number, we're removing the least significant bits.  We
  // Want to push them into the front of the array to preserve the order.
  var parts = [];
  while (bytes--) {
    parts.unshift("" + (temp_num % 256));
    temp_num -= temp_num % 256;
    temp_num /= 256;
  }

  return parts.join(".");
}

PROT_EnchashDecrypter.prototype.getLookupKey = function(host) {
  var dataKey = PROT_EnchashDecrypter.DATABASE_SALT + host;
  dataKey = this.base64_.arrayifyString(dataKey);

  this.hasher_.init(G_CryptoHasher.algorithms.MD5);
  var lookupDigest = this.hasher_.updateFromArray(dataKey);
  var lookupKey = this.hasher_.digestHex();

  return lookupKey.toUpperCase();
}

PROT_EnchashDecrypter.prototype.decryptData = function(data, host) {
  // XXX: base 64 decoding should be done in C++
  var asciiArray = this.base64_.decodeString(data);
  var ascii = this.base64_.stringifyArray(asciiArray);

  var random_salt = ascii.slice(0, PROT_EnchashDecrypter.SALT_LENGTH);
  var encrypted_data = ascii.slice(PROT_EnchashDecrypter.SALT_LENGTH);
  var temp_decryption_key = PROT_EnchashDecrypter.DATABASE_SALT
      + random_salt + host;
  this.hasher_.init(G_CryptoHasher.algorithms.MD5);
  this.hasher_.updateFromString(temp_decryption_key);

  var keyFactory = Cc["@mozilla.org/security/keyobjectfactory;1"]
                   .getService(Ci.nsIKeyObjectFactory);
  var key = keyFactory.keyFromString(Ci.nsIKeyObject.RC4,
                                     this.hasher_.digestRaw());

  this.streamCipher_.init(key);
  this.streamCipher_.updateFromString(encrypted_data);

  return this.streamCipher_.finish(false /* no base64 */);
}
