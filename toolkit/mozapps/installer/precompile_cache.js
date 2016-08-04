/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// see http://mxr.mozilla.org/mozilla-central/source/services/sync/Weave.js#76

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const rph = Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler);

function endsWith(str, end) {
  return str.slice(-end.length) == end;
}

function jar_entries(jarReader, pattern) {
  var entries = [];
  var enumerator = jarReader.findEntries(pattern);
  while (enumerator.hasMore()) {
    entries.push(enumerator.getNext());
  }
  return entries;
}

function dir_entries(baseDir, subpath, ext) {
  try {
    var dir = baseDir.clone();
    dir.append(subpath);
    var enumerator = dir.directoryEntries;
  } catch (e) {
    return [];
  }
  var entries = [];
  while (enumerator.hasMoreElements()) {
    var file = enumerator.getNext().QueryInterface(Ci.nsIFile);
    if (file.isDirectory()) {
      entries = entries.concat(dir_entries(dir, file.leafName, ext).map(p => subpath + "/" + p));
    } else if (endsWith(file.leafName, ext)) {
      entries.push(subpath + "/" + file.leafName);
    }
  }
  return entries;
}

function get_modules_under(uri) {
  if (uri instanceof Ci.nsIJARURI) {
    let jar = uri.QueryInterface(Ci.nsIJARURI);
    let jarReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
    let file = jar.JARFile.QueryInterface(Ci.nsIFileURL);
    jarReader.open(file.file);
    let entries = jar_entries(jarReader, "components/*.js")
                  .concat(jar_entries(jarReader, "modules/*.js"))
                  .concat(jar_entries(jarReader, "modules/*.jsm"));
    jarReader.close();
    return entries;
  } else if (uri instanceof Ci.nsIFileURL){
    let file = uri.QueryInterface(Ci.nsIFileURL);
    return dir_entries(file.file, "components", ".js")
           .concat(dir_entries(file.file, "modules", ".js"))
           .concat(dir_entries(file.file, "modules", ".jsm"));
  }
  throw new Error("Expected a nsIJARURI or nsIFileURL");
}

function load_modules_under(spec, uri) {
  var entries = get_modules_under(uri).sort();
  for (let entry of entries) {
    try {
      dump(spec + entry + "\n");
      Cu.import(spec + entry, null);
    } catch(e) {}
  }
}

function resolveResource(spec) {
  var uri = Services.io.newURI(spec, null, null);
  return Services.io.newURI(rph.resolveURI(uri), null, null);
}

function precompile_startupcache(uri) {
  load_modules_under(uri, resolveResource(uri));
}
