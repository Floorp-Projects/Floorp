/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 *  Taras Glek <tglek@mozilla.com>
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

// see http://mxr.mozilla.org/mozilla-central/source/services/sync/Weave.js#76

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

function setenv(name, val) {
  try {
    var environment = Components.classes["@mozilla.org/process/environment;1"].
      getService(Components.interfaces.nsIEnvironment);
    environment.set(name, val);
  } catch(e) {
    displayError("setenv", e);
  }
}

function load(url) {
  print(url);
  try {
    Cu.import(url, null);
  } catch(e) {
    dump("Failed to import " + url + ":" + e + "\n");
  }
}

function load_entries(entries, prefix) {
  while (entries.hasMore()) {
    var c = entries.getNext();
    load(prefix + c);
  }
}

function getGreDir() {
  return Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties).get("GreD", Ci.nsIFile);
}

function openJar(file) {
  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
    createInstance(Ci.nsIZipReader);
  zipreader.open(file);
  return zipreader;
}

function populate_startupcache(omnijarName, startupcacheName) {
  var file = getGreDir();
  file.append(omnijarName);
  zipreader = openJar(file);

  var scFile = getGreDir();
  scFile.append(startupcacheName);
  setenv("MOZ_STARTUP_CACHE", scFile.path);

  let prefix = "resource:///";

  load_entries(zipreader.findEntries("components/*js"), prefix);
  load_entries(zipreader.findEntries("modules/*js"), prefix);
  load_entries(zipreader.findEntries("modules/*jsm"), prefix);
  zipreader.close();
}
