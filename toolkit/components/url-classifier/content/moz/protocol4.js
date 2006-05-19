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


// A helper class that knows how to parse from and serialize to
// protocol4. This is a simple, historical format used by some Google
// interfaces, for example the Toolbar (i.e., ancient services).
//
// Protocol4 consists of a newline-separated sequence of name/value
// pairs (strings). Each line consists of the name, the value length,
// and the value itself, all separated by colons. Example:
//
// foo:6:barbaz\n
// fritz:33:issickofdynamicallytypedlanguages\n


/**
 * This class knows how to serialize/deserialize maps to/from their
 * protocol4 representation.
 *
 * @constructor
 */
function G_Protocol4Parser() {
  this.debugZone = "protocol4";

  this.protocol4RegExp_ = new RegExp("([^:]+):\\d+:(.*)$");
  this.newlineRegExp_ = new RegExp("(\\r)?\\n");
}

/**
 * Create a map from a protocol4 string. Silently skips invalid lines.
 *
 * @param text String holding the protocol4 representation
 * 
 * @returns Object as an associative array with keys and values 
 *          given in text. The empty object is returned if none
 *          are parsed.
 */
G_Protocol4Parser.prototype.parse = function(text) {

  var response = {};
  if (!text)
    return response;

  // Responses are protocol4: (repeated) name:numcontentbytes:content\n
  var lines = text.split(this.newlineRegExp_);
  for (var i = 0; i < lines.length; i++)
    if (this.protocol4RegExp_.exec(lines[i]))
      response[RegExp.$1] = RegExp.$2;

  return response;
}

/**
 * Create a protocol4 string from a map (object). Throws an error on 
 * an invalid input.
 *
 * @param map Object as an associative array with keys and values 
 *            given as strings.
 *
 * @returns text String holding the protocol4 representation
 */
G_Protocol4Parser.prototype.serialize = function(map) {
  if (typeof map != "object")
    throw new Error("map must be an object");

  var text = "";
  for (var key in map) {
    if (typeof map[key] != "string")
      throw new Error("Keys and values must be strings");
    
    text += key + ":" + map[key].length + ":" + map[key] + "\n";
  }
  
  return text;
}

#ifdef DEBUG
/**
 * Cheesey unittests
 */
function TEST_G_Protocol4Parser() {
  if (G_GDEBUG) {
    var z = "protocol4 UNITTEST";
    G_debugService.enableZone(z);

    G_Debug(z, "Starting");

    var p = new G_Protocol4Parser();
    
    function isEmpty(map) {
      for (var key in map) 
        return false;
      return true;
    };

    G_Assert(z, isEmpty(p.parse(null)), "Parsing null broken");
    G_Assert(z, isEmpty(p.parse("")), "Parsing nothing broken");

    var t = "foo:3:bar";
    G_Assert(z, p.parse(t)["foo"] === "bar", "Parsing one line broken");

    t = "foo:3:bar\n";
    G_Assert(z, p.parse(t)["foo"] === "bar", "Parsing line with lf broken");

    t = "foo:3:bar\r\n";
    G_Assert(z, p.parse(t)["foo"] === "bar", "Parsing with crlf broken");


    t = "foo:3:bar\nbar:3:baz\r\nbom:3:yaz\n";
    G_Assert(z, p.parse(t)["foo"] === "bar", "First in multiline");
    G_Assert(z, p.parse(t)["bar"] === "baz", "Second in multiline");
    G_Assert(z, p.parse(t)["bom"] === "yaz", "Third in multiline");
    G_Assert(z, p.parse(t)[""] === undefined, "Non-existent in multiline");
    
    // Test serialization

    var original = { 
      "1": "1", 
      "2": "2", 
      "foobar": "baz",
      "hello there": "how are you?" ,
    };
    var deserialized = p.parse(p.serialize(original));
    for (var key in original) 
      G_Assert(z, original[key] === deserialized[key], 
               "Trouble (de)serializing " + key);

    G_Debug(z, "PASSED");  
  }
}
#endif
