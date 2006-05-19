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
 *   Fritz Schneider <fritz@google.com> (original author)
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
// TODO: verify that using encodeURI() in getCanonicalHost is OK
// TODO: accommodate other kinds of perl-but-not-javascript qualifiers


/**
 * This thing knows how to generate lookup keys and decrypt values found in
 * a table of type enchash.
 */
function PROT_EnchashDecrypter() {
  this.debugZone = "enchashdecrypter";
  this.REs_ = PROT_EnchashDecrypter.REs;
  this.hasher_ = new G_CryptoHasher();
  this.base64_ = new G_Base64();
  this.rc4_ = new ARC4();
}

PROT_EnchashDecrypter.DATABASE_SALT = "oU3q.72p";
PROT_EnchashDecrypter.SALT_LENGTH = PROT_EnchashDecrypter.DATABASE_SALT.length;

PROT_EnchashDecrypter.MAX_DOTS = 5;

PROT_EnchashDecrypter.REs = {};
PROT_EnchashDecrypter.REs.FIND_DODGY_CHARS = 
  new RegExp("[\x01-\x1f\x7f-\xff]+");
PROT_EnchashDecrypter.REs.FIND_DODGY_CHARS_GLOBAL = 
  new RegExp("[\x01-\x1f\x7f-\xff]+", "g");
PROT_EnchashDecrypter.REs.FIND_END_DOTS = new RegExp("^\\.+|\\.+$");
PROT_EnchashDecrypter.REs.FIND_END_DOTS_GLOBAL = 
  new RegExp("^\\.+|\\.+$", "g");
PROT_EnchashDecrypter.REs.FIND_MULTIPLE_DOTS = new RegExp("\\.{2,}");
PROT_EnchashDecrypter.REs.FIND_MULTIPLE_DOTS_GLOBAL = 
  new RegExp("\\.{2,}", "g");
PROT_EnchashDecrypter.REs.FIND_TRAILING_DOTS = new RegExp("\\.+$");
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

PROT_EnchashDecrypter.prototype.getCanonicalHost = function(str) {
  var urlObj = Cc["@mozilla.org/network/standard-url;1"]
               .createInstance(Ci.nsIURL);
  urlObj.spec = str;
  var asciiHost = urlObj.asciiHost;

  var unescaped = this.hexDecode_(asciiHost);

  unescaped = unescaped.replace(this.REs_.FIND_DODGY_CHARS_GLOBAL, "")
              .replace(this.REs_.FIND_END_DOTS_GLOBAL, "")
              .replace(this.REs_.FIND_MULTIPLE_DOTS_GLOBAL, ".");

  var temp = this.parseIPAddress_(unescaped);
  if (temp)
    unescaped = temp;

  // TODO: what, exactly is it supposed to escape? This doesn't esecape 
  // ":", "/", ";", and "?"
  var escaped = encodeURI(unescaped);

  var k;
  var index = escaped.length;
  for (k = 0; k < PROT_EnchashDecrypter.MAX_DOTS + 1; k++) {
    temp = escaped.lastIndexOf(".", index - 1);
    if (temp == -1) {
      break;
    } else {
      index = temp;
    }
  }
  
  if (k == PROT_EnchashDecrypter.MAX_DOTS + 1 && index != -1) {
    escaped = escaped.substring(index + 1);
  }

  escaped = escaped.toLowerCase();
  return escaped;
}

PROT_EnchashDecrypter.prototype.parseIPAddress_ = function(host) {

  host = host.replace(this.REs_.FIND_TRAILING_DOTS_GLOBAL, "");

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

    num = this.lastNChars_(num, 8);

    temp_num = parseInt(num, 16);
    if (isNaN(temp_num))
      temp_num = -1;

  } else {
    return "";
  }

  if (temp_num == -1) 
    return "";

  var parts = [];
  while (bytes--) {
    parts.push("" + (temp_num % 256));
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

  var ascii = this.base64_.decodeString(data);
  var random_salt = ascii.slice(0, PROT_EnchashDecrypter.SALT_LENGTH);
  var encrypted_data = ascii.slice(PROT_EnchashDecrypter.SALT_LENGTH);
  var temp_decryption_key = 
    this.base64_.arrayifyString(PROT_EnchashDecrypter.DATABASE_SALT);
  temp_decryption_key = temp_decryption_key.concat(random_salt);
  temp_decryption_key = 
    temp_decryption_key.concat(this.base64_.arrayifyString(host));
                               
  this.hasher_.init(G_CryptoHasher.algorithms.MD5);
  this.hasher_.updateFromArray(temp_decryption_key);
  var decryption_key = this.base64_.arrayifyString(this.hasher_.digestRaw());

  this.rc4_.setKey(decryption_key, decryption_key.length);
  this.rc4_.crypt(encrypted_data, encrypted_data.length);   // Works in-place
  
  return this.base64_.stringifyArray(encrypted_data);
}

#ifdef DEBUG
/**
 * Lame unittesting function
 */
function TEST_PROT_EnchashDecrypter() {
  if (G_GDEBUG) {
    var z = "enchash UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");  

    // Yes this defies our naming convention, but we copy verbatim from 
    // the C++ unittest, so lets just keep things clear.
    var no_dots = "abcd123;[]";
    var one_dot = "abc.123";
    var two_dots = "two..dots";
    var lots_o_dots = "I have a lovely .... bunch of dots";
    var multi_dots = "dots ... and ... more .... dots";
    var leading_dot = ".leading";
    var trailing_dot = "trailing.";
    var trailing_dots = "I love trailing dots....";
    var end_dots = ".dots.";

    var decimal = "1234567890";
    var hex = "0x123452FAf";
    var bad_hex = "0xFF0xGG";
    var octal = "012034056";
    var bad_octal = "012034089";
    var garbage = "lk,.:asdfa-=";
    var mixed = "1230x78034";
    var spaces = "123 0xFA 045";
    
    var longstr = "";
    for(var k = 0; k < 100; k++) {
      longstr += "a";
    }

    var shortstr = "short";

    var r = PROT_EnchashDecrypter.REs;
    var l = new PROT_EnchashDecrypter();

    // Test regular expressions
    function testRE(re, inputValPairs) {
      for (var i = 0; i < inputValPairs.length; i += 2) 
        G_Assert(z, re.test(inputValPairs[i]) == inputValPairs[i + 1], 
                 "RegExp broken: " + re + " (input: " + inputValPairs[i] + ")");
    };

    var tests = 
      ["", false, 
       "normal chars;!@#$%^&*&(", false,
       "MORE NORMAL ,./<>?;':{}", false,
       "Slightly less\2 normal", true, 
       "\245 stuff \45", true, 
       "\31", true];
    testRE(r.FIND_DODGY_CHARS, tests);

    tests = 
      [no_dots, false, 
       one_dot, false, 
       leading_dot, true, 
       trailing_dots, true, 
       end_dots, true];
    testRE(r.FIND_END_DOTS, tests);

    tests =
      [no_dots, false,
       one_dot, false,
       two_dots, true, 
       lots_o_dots, true,
       multi_dots, true];
    testRE(r.FIND_MULTIPLE_DOTS, tests);

    tests = 
      [no_dots, false, 
       one_dot, false,
       trailing_dot, true,
       trailing_dots, true];
    testRE(r.FIND_TRAILING_DOTS, tests);

    tests = 
      ["random junk", false,
       "123.45.6-7.89", false,
       "012.12.123", true,
       "0x12.0xff.123", true,
       "225.0.0.1", true];
    testRE(r.POSSIBLE_IP, tests);

    tests = 
      [decimal, false,
       hex, false, 
       octal, false, 
       bad_octal, true];
    testRE(r.FIND_BAD_OCTAL, tests);

    tests = 
      [decimal, false, 
       hex, false, 
       bad_octal, false,
       garbage, false,
       mixed, false,
       spaces, false,
       octal, true];
    testRE(r.IS_OCTAL, tests);

    tests = 
      [hex, false,
       garbage, false, 
       mixed, false, 
       spaces, false, 
       octal, true,
       bad_octal, true,
       decimal, true];
    testRE(r.IS_DECIMAL, tests);

    tests =
      [decimal, false, 
       octal, false, 
       bad_octal, false,
       garbage, false,
       mixed, false,
       spaces, false,
       bad_hex, false,
       hex, true];
    testRE(r.IS_HEX, tests);

    // Test find last N
    var val = l.lastNChars_(longstr, 8);
    G_Assert(z, val.length == 8, "find last eight broken on long str");
    val = l.lastNChars_(shortstr, 8);
    G_Assert(z, val.length == 5, "find last 11 broken on short str");

    // Test canonical num
    tests = 
      ["", "", 1, true, 
       "", "10", 0, true,
       "", "0x45", -1, true,
       "45", "45", 1, true,
       "16", "0x10", 1, true,
       "111.1", "367", 2, true,
       "229.20.0", "012345", 3, true,
       "123", "0173", 1, true,
       "9", "09", 1, false,
       "", "0x120x34", 2, true,
       "252.18", "0x12fc", 2, true];
    for (var i = 0; i < tests.length; i+= 4)
      G_Assert(z, tests[i] === l.canonicalNum_(tests[i + 1], 
                                               tests[i + 2], 
                                               tests[i + 3]),
               "canonicalNum broken on: " + tests[i + 1]);

    // Test parseIPAddress
    var testing = {};
    testing["123.123.0.0.1"] = "";
    testing["255.0.0.1"] = "255.0.0.1";
    testing["12.0x12.01234"] = "12.18.156.2";
    testing["276.2.3"] = "20.2.3.0";
    testing["012.034.01.055"] = "10.28.1.45";
    testing["0x12.0x43.0x44.0x01"] = "18.67.68.1";
    
    for (var key in testing) 
      G_Assert(z, l.parseIPAddress_(key) === testing[key], 
               "parseIPAddress broken on " + key + "(got: " +
               l.parseIPAddress_(key));

    // Test getCanonicalHost
    var testing = {};
    testing["completely.bogus.url.with.a.whole.lot.of.dots"] = 
      "with.a.whole.lot.of.dots";
    testing["http://poseidon.marinet.gr/~elani"] = "poseidon.marinet.gr";
    testing["http://www.google.com.."] = "www.google.com";
    testing["https://www.yaho%6F.com"] = "www.yahoo.com";
    testing["http://012.034.01.0xa"] = "10.28.1.10";
    testing["ftp://wierd..chars...%0f,%fa"] = "wierd.chars.,";
    for (var key in testing)
      G_Assert(z, l.getCanonicalHost(key) == testing[key], 
               "getCanonicalHost broken on: " + key + 
               "(got: " + l.getCanonicalHost(key) + ")");

    // Test getlookupkey
    var testing = {};
    testing["www.google.com"] = "AF5638A09FDDDAFF5B7A6013B1BE69A9";
    testing["poseidon.marinet.gr"] = "01844755C8143C4579BB28DD59C23747";
    testing["80.53.164.26"] = "B775DDC22DEBF8BEBFEAC24CE40A1FBF";

    for (var key in testing)
      G_Assert(z, l.getLookupKey(key) === testing[key], 
               "getlookupkey broken on " + key + " (got: " + 
               l.getLookupKey(key) + ", expected: " + 
               testing[key] + ")");

    // Test decryptdata
    var tests = 
      [ "bGtEQWJuMl/z2ZxSBB2hsuWI8geMAwfSh3YBfYPejQ1O+wyRAJeJ1UW3V56zm" +
        "EpUvnaEiECN1pndxW5rEMNzE+gppPeel7PvH+OuabL3NXlspcP0xnpK8rzNgB1" +
        "JT1KcajQ9K3CCl24T9r8VGb0M3w==", 
        "80.53.164.26", 
        "^(?i)http\\:\\/\\/80\\.53\\.164\\.26(?:\\:80)?\\/\\.PayPal" +
        "\\.com\\/webscr\\-id\\/secure\\-SSL\\/cmd\\-run\\=\\/login\\.htm$",

        "ZTMzZjVnb3WW1Yc2ABorgQGAwYfcaCb/BG3sMFLTMDvOQxH8LkdGGWqp2tI5SK" +
        "uNrXIHNf2cyzcVocTqUIUkt1Ud1GKieINcp4tWcU53I0VZ0ZZHCjGObDCbv9Wb" +
        "CPSx1eS8vMREDv8Jj+UVL1yaZQ==", 
        "80.53.164.26", 
        "^(?i)http\\:\\/\\/80\\.53\\.164\\.26(?:\\:80)?\\/\\.PayPal\\.com" +
        "\\/webscr\\-id\\/secure\\-SSL\\/cmd\\-run\\=\\/login\\.htm$",

        "ZTMzZjVnb3WVb6VqoJ44hVo4V77XjDRcXTxOc2Zpn4yIHcpS0AQ0nn1TVlX4MY" +
        "IeNL/6ggzCmcJSWOOkj06Mpo56LNLrbxNxTBuoy9GF+xcm", 
        "poseidon.marinet.gr", 
        "^(?i)http\\:\\/\\/poseidon\\.marinet\\.gr(?:\\:80)?\\/\\~eleni" +
        "\\/eBay\\/index\\.php$",

        "bGtEQWJuMl9FA3Kl5RiXMpgFU8nDJl9J0hXjUck9+mMUQwAN6llf0gJeY5DIPP" +
        "c2f+a8MSBFJN17ANGJZl5oZVsQfSW4i12rlScsx4tweZAE", 
        "poseidon.marinet.gr", 
        "^(?i)http\\:\\/\\/poseidon\\.marinet\\.gr(?:\\:80)?\\/\\~eleni" +
        "\\/eBay\\/index\\.php$"];

    for (var i = 0; i < tests.length; i += 3) {
      var dec = l.decryptData(tests[i], tests[i + 1]);
      G_Assert(z, dec === tests[i + 2],
               "decryptdata broken on " + tests[i] + " (got: " + dec + 
               ", expected: " + tests[i + 2] + ")");
    }


    G_Debug(z, "PASSED");
  }
}
#endif
