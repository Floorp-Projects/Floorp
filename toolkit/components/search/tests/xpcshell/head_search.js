/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/FileUtils.jsm");
Components.utils.import("resource://gre/modules/Promise.jsm");
Components.utils.import("resource://testing-common/AppInfo.jsm");

const BROWSER_SEARCH_PREF = "browser.search.";
const NS_APP_SEARCH_DIR = "SrchPlugns";

const MODE_RDONLY = FileUtils.MODE_RDONLY;
const MODE_WRONLY = FileUtils.MODE_WRONLY;
const MODE_CREATE = FileUtils.MODE_CREATE;
const MODE_TRUNCATE = FileUtils.MODE_TRUNCATE;

// Need to create and register a profile folder.
var gProfD = do_get_profile();

function dumpn(text)
{
  dump("search test: " + text + "\n");
}

/**
 * Clean the profile of any metadata files left from a previous run.
 */
function removeMetadata()
{
  let file = gProfD.clone();
  file.append("search-metadata.json");
  if (file.exists()) {
    file.remove(false);
  }

  file = gProfD.clone();
  file.append("search.sqlite");
  if (file.exists()) {
    file.remove(false);
  }
}

function removeCacheFile()
{
  let file = gProfD.clone();
  file.append("search.json");
  if (file.exists()) {
    file.remove(false);
  }
}

/**
 * Clean the profile of any cache file left from a previous run.
 */
function removeCache()
{
  let file = gProfD.clone();
  file.append("search.json");
  if (file.exists()) {
    file.remove(false);
  }

}

/**
 * Run some callback once metadata has been committed to disk.
 */
function afterCommit(callback)
{
  let obs = function(result, topic, verb) {
    if (verb == "write-metadata-to-disk-complete") {
      Services.obs.removeObserver(obs, topic);
      callback(result);
    } else {
      dump("TOPIC: " + topic+ "\n");
    }
  }
  Services.obs.addObserver(obs, "browser-search-service", false);
}

/**
 * Run some callback once cache has been built.
 */
function afterCache(callback)
{
  let obs = function(result, topic, verb) {
    do_print("afterCache: " + verb);
    if (verb == "write-cache-to-disk-complete") {
      Services.obs.removeObserver(obs, topic);
      callback(result);
    } else {
      dump("TOPIC: " + topic+ "\n");
    }
  }
  Services.obs.addObserver(obs, "browser-search-service", false);
}

function parseJsonFromStream(aInputStream) {
  const json = Cc["@mozilla.org/dom/json;1"].createInstance(Components.interfaces.nsIJSON);
  const data = json.decodeFromStream(aInputStream, aInputStream.available());
  return data;
}

/**
 * Read a JSON file and return the JS object
 */
function readJSONFile(aFile) {
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].
               createInstance(Ci.nsIFileInputStream);
  try {
    stream.init(aFile, MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    return parseJsonFromStream(stream, stream.available());
  } catch(ex) {
    dumpn("readJSONFile: Error reading JSON file: " + ex);
  } finally {
    stream.close();
  }
  return false;
}

/**
 * Recursively compare two objects and check that every property of expectedObj has the same value
 * on actualObj.
 */
function isSubObjectOf(expectedObj, actualObj) {
  for (let prop in expectedObj) {
    if (expectedObj[prop] instanceof Object) {
      do_check_eq(expectedObj[prop].length, actualObj[prop].length);
      isSubObjectOf(expectedObj[prop], actualObj[prop]);
    } else {
      do_check_eq(expectedObj[prop], actualObj[prop]);
    }
  }
}

// Expand the amount of information available in error logs
Services.prefs.setBoolPref("browser.search.log", true);

