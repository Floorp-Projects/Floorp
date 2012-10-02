/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function getDir(prop) {
  return Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties).get(prop, Ci.nsIFile);
}

function openJar(file) {
  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
    createInstance(Ci.nsIZipReader);
  zipreader.open(file);
  return zipreader;
}

function populate_startupcache(prop, omnijarName, startupcacheName) {
  var file = getDir(prop);
  file.append(omnijarName);
  zipreader = openJar(file);

  var scFile = getDir(prop);
  scFile.append(startupcacheName);
  setenv("MOZ_STARTUP_CACHE", scFile.path);

  let prefix = "resource:///";

  load_entries(zipreader.findEntries("components/*js"), prefix);
  load_entries(zipreader.findEntries("modules/*js"), prefix);
  load_entries(zipreader.findEntries("modules/*jsm"), prefix);
  zipreader.close();
}
