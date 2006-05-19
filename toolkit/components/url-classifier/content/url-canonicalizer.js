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


// This is the class we use to canonicalize URLs for TRTables of type
// url.  We maximally URL-decode the URL, treating +'s as if they're
// not special.  We then specially URL-encode it (we encode ASCII
// values [0, 32] (whitespace or unprintable), 37 (%), [127, 255]
// (unprintable)).  
//
// This mapping is not a function. That is, multiple URLs can map to
// the same canonical representation. However this is OK because
// collisions happen only when there are weird characters (e.g.,
// nonprintables), and the canonical representation makes us robust
// to some weird kinds of encoding we could see. 
//
// All members are static at this point -- this is basically a namespace.


/**
 * Create a new URLCanonicalizer. Useless because members are static.
 *
 * @constructor
 */
function PROT_URLCanonicalizer() { 
  throw new Error("No need to instantiate a canonicalizer at this point.");
}

PROT_URLCanonicalizer.debugZone = "urlcanonicalizer";

PROT_URLCanonicalizer.hexChars_ = "0123456789ABCDEF";

/**
 * Helper funciton to (maybe) convert a two-character hex string into its 
 * decimal numerical equivalent
 *
 * @param hh String of length two that might be a valid hex sequence
 *
 * @returns Number: NaN if hh wasn't valid hex, else the appropriate decimal
 *          value
 */
PROT_URLCanonicalizer.hexPairToInt_ = function(hh) {
  return Number("0x" + hh);
}

/**
 * Helper function to hex-encode a number
 *
 * @param val Number in range [0, 255]
 *
 * @returns String containing the hex representation of that number (sans 0x)
 */
PROT_URLCanonicalizer.toHex_ = function(val) {
  var retVal = PROT_URLCanonicalizer.hexChars_.charAt((val >> 4) & 15) + 
               PROT_URLCanonicalizer.hexChars_.charAt(val & 15);
  return retVal;
}

/**
 * Get the canonical version of the given URL for lookup in a table of 
 * type -url.
 *
 * @param url String to canonicalize
 *
 * @returns String containing the canonicalized url (maximally url-decoded,
 *          then specially url-encoded)
 */
PROT_URLCanonicalizer.canonicalizeURL_ = function(url) {
  var arrayOfASCIIVals = PROT_URLCanonicalizer.fullyDecodeURL_(url);
  return PROT_URLCanonicalizer.specialEncodeURL_(arrayOfASCIIVals);
}

/**
 * Maximally URL-decode a URL. This breaks the semantics of the URL, but
 * we don't care because we're using it for lookup, not for navigation.
 * We break multi-byte UTF-8 escape sequences as well, but we don't care
 * so long as they canonicalize the same way consistently (they do).
 *
 * @param url String containing the URL to maximally decode. Should ONLY
 *            contain characters with UCS codepoints U+0001 to U+00FF
 *            (the ASCII set minus null).
 *
 * @returns Array of ASCII values corresponding to the decoded sequence of
 *          characters in the url
 */
PROT_URLCanonicalizer.fullyDecodeURL_ = function(url) {

  // The goals here are: simplicity, correctness, and most of all
  // portability; we want the same implementation of canonicalization
  // wherever we use it so as to to minimize the chances of
  // inconsistency. For example, we have to do this canonicalization
  // on URLs we get from third parties, and at the lookup server when 
  // we get a request.
  //
  // The following implementation should translate easily to any
  // language that supports arrays and pointers or references. Note
  // that arrays are pointer types in JavaScript, so foo = [some,
  // array] points foo at the array; it doesn't copy it. The
  // implementation is efficient (linear) so long as most %'s in the
  // url belong to valid escape sequences and there aren't too many
  // doubly-escaped values.

  // The basic idea is to copy current input to output, decoding escape 
  // sequences as we see them, until we decode a %. At that point we start
  // copying into the "next iteration buffer" instead of the output buffer; 
  // we do this so we can accomodate multiply-escaped strings. When we hit 
  // the end of the input, we take the "next iteration buffer" as our input,
  // and start over.

  var nextIteration = url.split("");
  var output = [];

  while (nextIteration.length) {

    var decodedAPercent = false;
    var thisIteration = nextIteration;
    var nextIteration = [];
    
    var i = 0;
    while (i < thisIteration.length) {

      var c = thisIteration[i];
      if (c == "%" && i + 2 < thisIteration.length) {

        // Peek ahead to see if we have a valid HH sequence
        var asciiVal = 
          PROT_URLCanonicalizer.hexPairToInt_(thisIteration[i + 1] + 
                                              thisIteration[i + 2]);
        if (!isNaN(asciiVal)) {
          i += 2;                   // Valid HH sequence; consume it
          
          if (asciiVal == 0)        // We special case nulls
            asciiVal = 1;
          
          c = String.fromCharCode(asciiVal);
          if (c == "%")
            decodedAPercent = true;
        }
      }

      if (decodedAPercent)
        nextIteration[nextIteration.length] = c;
      else
        output[output.length] = c.charCodeAt(0);
      
      ++i;
    }
  }

  return output;
}

/**
 * Maximally URL-decode a URL (same as fullyDecodeURL_ except that it 
 * returns a string). Useful for making unittests more readable.
 *
 * @param url String containing the URL to maximally decode. Should ONLY
 *            contain characters with UCS codepoints U+0001 to U+00FF
 *            (the ASCII set minus null).
 *
 * @returns String containing the decoded URL
 */
PROT_URLCanonicalizer.fullyDecodeURLAsString_ = function(url) {
  var arrayOfASCIIVals = PROT_URLCanonicalizer.fullyDecodeURL_(url);
  var s = "";
  for (var i = 0; i < arrayOfASCIIVals.length; i++)
    s += String.fromCharCode(arrayOfASCIIVals[i]);
  return s;
}

/**
 * Specially URL-encode the given array of ASCII values. We want to encode 
 * the charcters: [0, 32], 37, [127, 255].
 *
 * @param arrayOfASCIIValues Array of ascii values (numbers) to encode
 *
 * @returns String corresonding to the escaped URL
 */
PROT_URLCanonicalizer.specialEncodeURL_ = function(arrayOfASCIIValues) {

  var output = [];
  for (var i = 0; i < arrayOfASCIIValues.length; i++) {
    var n = arrayOfASCIIValues[i];

    if (n <= 32 || n == 37 || n >= 127)
      output.push("%" + ((!n) ? "01" : PROT_URLCanonicalizer.toHex_(n)));
    else
      output.push(String.fromCharCode(n));
  }

  return output.join("");
}


#ifdef DEBUG
/**
 * Lame unittesting function
 */
function TEST_PROT_URLCanonicalizer() {
  if (G_GDEBUG) {
    var z = "urlcanonicalizer UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");  


    // ------ TEST HEX GOTCHA ------

    var hexify = PROT_URLCanonicalizer.toHex_;
    var shouldHaveLeadingZeros = hexify(0) + hexify(1);
    G_Assert(z, shouldHaveLeadingZeros == "0001",
             "Need to append leading zeros to hex rep value <= 15 !")


    // ------ TEST DECODING ------
    
    // For convenience, shorten the function name
    var dec = PROT_URLCanonicalizer.fullyDecodeURLAsString_;

    // Test empty string
    G_Assert(z, dec("") == "", "decoding empty string");

    // Test decoding of all characters
    var allCharsEncoded = "";
    var allCharsEncodedLowercase = "";
    var allCharsAsString = "";
    // Special case null
    allCharsEncoded += "%01";
    allCharsEncodedLowercase += "%01";
    allCharsAsString += String.fromCharCode(1);
    for (var i = 1; i < 256; i++) {
      allCharsEncoded += "%" + PROT_URLCanonicalizer.toHex_(i);
      allCharsEncodedLowercase += "%" + 
                               PROT_URLCanonicalizer.toHex_(i).toLowerCase();
      allCharsAsString += String.fromCharCode(i);
    }
    G_Assert(z, dec(allCharsEncoded) == allCharsAsString, "decoding escaped");
    G_Assert(z, dec(allCharsEncodedLowercase) == allCharsAsString, 
             "decoding lowercase");

    // Test %-related edge cases
    G_Assert(z, dec("%") == "%", "1 percent");
    G_Assert(z, dec("%xx") == "%xx", "1 percent, two non-hex");
    G_Assert(z, dec("%%") == "%%", "2 percent");
    G_Assert(z, dec("%%%") == "%%%", "3 percent");
    G_Assert(z, dec("%%%%") == "%%%%", "4 percent");
    G_Assert(z, dec("%1") == "%1", "1 percent, one nonhex");
    G_Assert(z, dec("%1z") == "%1z", "1 percent, two nonhex");
    G_Assert(z, dec("a%1z") == "a%1z", "nonhex, 1 percent, two nonhex");
    G_Assert(z, dec("abc%d%e%fg%hij%klmno%") == "abc%d%e%fg%hij%klmno%",
             "lots of percents, no hex");

    // Test repeated %-decoding. Note: %25 --> %, %32 --> 2, %35 --> 5
    G_Assert(z, dec("%25") == "%", "single-encoded %");
    G_Assert(z, dec("%25%32%35") == "%", "double-encoded %");
    G_Assert(z, dec("asdf%25%32%35asd") == "asdf%asd", "double-encoded % 2");
    G_Assert(z, dec("%%%25%32%35asd%%") == "%%%asd%%", "double-encoded % 3");
    G_Assert(z, dec("%25%32%35%25%32%35%25%32%35") == "%%%", 
             "sequenctial double-encoded %");
    G_Assert(z, dec("%2525252525252525") == "%", "many-encoded %");
    G_Assert(z, dec("%257Ea%2521b%2540c%2523d%2524e%25f%255E00%252611%252A22%252833%252944_55%252B") == "~a!b@c#d$e%f^00&11*22(33)44_55+",
             "4x-encoded string");


    // ------ TEST ENCODING ------

    // For convenience, shorten the function name
    var enc = PROT_URLCanonicalizer.specialEncodeURL_;

    // Test empty string
    G_Assert(z, enc([]) == "", "encoding empty array");

    // Test that all characters we shouldn't encode ([33-36],[38,126]) are not.
    var no = [];
    var noAsString = "";
    for (var i = 33; i < 127; i++)
      if (i != 37) {                      // skip %
        no.push(i);
        noAsString += String.fromCharCode(i);
      }
    G_Assert(z, enc(no) == noAsString, "chars to not encode");

    // Test that all the chars that we should encode [0,32],37,[127,255] are
    var yes = [];
    var yesAsString = "";
    var yesExpectedString = "";
    // Special case 0 
    yes.push(0);
    yesAsString += String.fromCharCode(1);
    yesExpectedString += "%01";
    for (var i = 1; i < 256; i++) 
      if (i < 33 || i == 37 || i > 126) {
        yes.push(i);
        yesAsString += String.fromCharCode(i);
        var hex = i.toString(16).toUpperCase();
        yesExpectedString += "%" + ((i < 16) ? "0" : "") + hex;
      }
    G_Assert(z, enc(yes) == yesExpectedString, "chars to encode");
    // Can't use decodeURIComponent or encodeURIComponent to test b/c UTF-8


    // ------ TEST COMPOSITION ------

    // For convenience, shorten function name:
    var c = PROT_URLCanonicalizer.canonicalizeURL_;
    
    G_Assert(z, c("http://www.google.com") == "http://www.google.com", 
             "http://www.google.com");
    G_Assert(z, c("http://%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/%77%77%77%2E%65%62%61%79%2E%63%6F%6D/") == "http://168.188.99.26/.secure/www.ebay.com/", 
             "fully encoded ebay");
    G_Assert(z, c("http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/") == "http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/", 
             "long url with spaces that stays same");

    // TODO: MORE!

    G_Debug(z, "PASSED");
  }
}
#endif
