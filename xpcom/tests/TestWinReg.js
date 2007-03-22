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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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
