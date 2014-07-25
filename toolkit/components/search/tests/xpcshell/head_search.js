/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://testing-common/AppInfo.jsm");
Cu.import("resource://testing-common/httpd.js");

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

function getSearchMetadata()
{
  // Check that search-metadata.json has been created
  let metadata = gProfD.clone();
  metadata.append("search-metadata.json");
  do_check_true(metadata.exists());

  do_print("Parsing metadata");
  return readJSONFile(metadata);
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

/**
 * After useHttpServer() is called, this string contains the URL of the "data"
 * directory, including the final slash.
 */
let gDataUrl;

/**
 * Initializes the HTTP server and ensures that it is terminated when tests end.
 *
 * @return The HttpServer object in case further customization is needed.
 */
function useHttpServer() {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  gDataUrl = "http://localhost:" + httpServer.identity.primaryPort + "/data/";
  do_register_cleanup(() => httpServer.stop(() => {}));
  return httpServer;
}

/**
 * Adds test engines and returns a promise resolved when they are installed.
 *
 * The engines are added in the given order.
 *
 * @param aItems
 *        Array of objects with the following properties:
 *        {
 *          name: Engine name, used to wait for it to be loaded.
 *          xmlFileName: Name of the XML file in the "data" folder.
 *          srcFileName: Name of the SRC file in the "data" folder.
 *          iconFileName: Name of the icon associated to the SRC file.
 *          details: Array containing the parameters of addEngineWithDetails,
 *                   except for the engine name.  Alternative to xmlFileName.
 *        }
 */
let addTestEngines = Task.async(function* (aItems) {
  if (!gDataUrl) {
    do_throw("useHttpServer must be called before addTestEngines.");
  }

  let engines = [];

  for (let item of aItems) {
    do_print("Adding engine: " + item.name);
    yield new Promise((resolve, reject) => {
      Services.obs.addObserver(function obs(subject, topic, data) {
        try {
          let engine = subject.QueryInterface(Ci.nsISearchEngine);
          do_print("Observed " + data + " for " + engine.name);
          if (data != "engine-added" || engine.name != item.name) {
            return;
          }

          Services.obs.removeObserver(obs, "browser-search-engine-modified");
          engines.push(engine);
          resolve();
        } catch (ex) {
          reject(ex);
        }
      }, "browser-search-engine-modified", false);

      if (item.xmlFileName) {
        Services.search.addEngine(gDataUrl + item.xmlFileName,
                                  Ci.nsISearchEngine.DATA_XML, null, false);
      } else if (item.srcFileName) {
        Services.search.addEngine(gDataUrl + item.srcFileName,
                                  Ci.nsISearchEngine.DATA_TEXT,
                                  gDataUrl + item.iconFileName, false);
      } else {
        Services.search.addEngineWithDetails(item.name, ...item.details);
      }
    });
  }

  return engines;
});
