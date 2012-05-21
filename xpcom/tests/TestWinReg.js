/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This script is intended to be run using xpcshell
 */

const nsIWindowsRegKey = Components.interfaces.nsIWindowsRegKey;
const BASE_PATH = "SOFTWARE\\Mozilla\\Firefox";

function idump(indent, str)
{
  for (var j = 0; j < indent; ++j)
    dump(" ");
  dump(str);
}

function list_values(indent, key) {
  idump(indent, "{\n");
  var count = key.valueCount;
  for (var i = 0; i < count; ++i) {
    var vn = key.getValueName(i);
    var val = "";
    if (key.getValueType(vn) == nsIWindowsRegKey.TYPE_STRING) {
      val = key.readStringValue(vn);
    }
    if (vn == "") 
      idump(indent + 1, "(Default): \"" + val + "\"\n");
    else
      idump(indent + 1, vn + ": \"" + val + "\"\n");
  } 
  idump(indent, "}\n");
}

function list_children(indent, key) {
  list_values(indent, key);

  var count = key.childCount;
  for (var i = 0; i < count; ++i) {
    var cn = key.getChildName(i);
    idump(indent, "[" + cn + "]\n");
    list_children(indent + 1, key.openChild(cn, nsIWindowsRegKey.ACCESS_READ));
  }
}

// enumerate everything under BASE_PATH
var key = Components.classes["@mozilla.org/windows-registry-key;1"].
    createInstance(nsIWindowsRegKey);
key.open(nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, BASE_PATH,
         nsIWindowsRegKey.ACCESS_READ);
list_children(1, key);

key.close();
key.open(nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, BASE_PATH,
         nsIWindowsRegKey.ACCESS_READ);
list_children(1, key);
