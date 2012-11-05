/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [
  "parseKeyValuePairs",
  "parseKeyValuePairsFromFile"
];

const Cc = Components.classes;
const Ci = Components.interfaces;

this.parseKeyValuePairs = function parseKeyValuePairs(text) {
  let lines = text.split('\n');
  let data = {};
  for (let i = 0; i < lines.length; i++) {
    if (lines[i] == '')
      continue;

    // can't just .split() because the value might contain = characters
    let eq = lines[i].indexOf('=');
    if (eq != -1) {
      let [key, value] = [lines[i].substring(0, eq),
                          lines[i].substring(eq + 1)];
      if (key && value)
        data[key] = value.replace(/\\n/g, "\n").replace(/\\\\/g, "\\");
    }
  }
  return data;
}

this.parseKeyValuePairsFromFile = function parseKeyValuePairsFromFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let is = Cc["@mozilla.org/intl/converter-input-stream;1"].
           createInstance(Ci.nsIConverterInputStream);
  is.init(fstream, "UTF-8", 1024, Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
  let str = {};
  let contents = '';
  while (is.readString(4096, str) != 0) {
    contents += str.value;
  }
  is.close();
  fstream.close();
  return parseKeyValuePairs(contents);
}
