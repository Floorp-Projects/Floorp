// ***** BEGIN LICENSE BLOCK *****// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Corporation Code.
//
// The Initial Developer of the Original Code is
// Mikeal Rogers.
// Portions created by the Initial Developer are Copyright (C) 2008
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//  Mikeal Rogers <mikeal.rogers@gmail.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****

var EXPORTED_SYMBOLS = ['listDirectory', 'getFileForPath', 'abspath', 'getPlatform'];

function listDirectory (file) {
  // file is the given directory (nsIFile)
  var entries = file.directoryEntries;
  var array = [];
  while (entries.hasMoreElements())
  {
    var entry = entries.getNext();
    entry.QueryInterface(Components.interfaces.nsIFile);
    array.push(entry);
  }
  return array;
}

function getFileForPath (path) {
  var file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(path);
  return file;
}

function abspath (rel, file) {
  var relSplit = rel.split('/');
  if (relSplit[0] == '..' && !file.isDirectory()) {
    file = file.parent;
  }
  for each(p in relSplit) {
    if (p == '..') {
      file = file.parent;
    } else if (p == '.'){
      if (!file.isDirectory()) {
        file = file.parent;
      }
    } else {
      file.append(p);
    }
  }
  return file.path;
}

function getPlatform () {
  var xulRuntime = Components.classes["@mozilla.org/xre/app-info;1"]
                   .getService(Components.interfaces.nsIXULRuntime);
  mPlatform = xulRuntime.OS.toLowerCase();
  return mPlatform;
}


