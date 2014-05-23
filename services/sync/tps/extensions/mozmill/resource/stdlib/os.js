/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['listDirectory', 'getFileForPath', 'abspath', 'getPlatform'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

function listDirectory(file) {
  // file is the given directory (nsIFile)
  var entries = file.directoryEntries;
  var array = [];

  while (entries.hasMoreElements()) {
    var entry = entries.getNext();
    entry.QueryInterface(Ci.nsIFile);
    array.push(entry);
  }

  return array;
}

function getFileForPath(path) {
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return file;
}

function abspath(rel, file) {
  var relSplit = rel.split('/');

  if (relSplit[0] == '..' && !file.isDirectory()) {
    file = file.parent;
  }

  for each(var p in relSplit) {
    if (p == '..') {
      file = file.parent;
    } else if (p == '.') {
      if (!file.isDirectory()) {
        file = file.parent;
      }
    } else {
      file.append(p);
    }
  }

  return file.path;
}

function getPlatform() {
  return Services.appinfo.OS.toLowerCase();
}
