/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

/* import-globals-from manifestLibrary.js */

// Defined in browser-test.js
/* global gTestPath */

/*
 * getChromeURI converts a URL to a URI
 *
 * url: string of a URL (http://mochi.test/test.html)
 * returns: a nsiURI object representing the given URL
 *
 */
function getChromeURI(url) {
  return Services.io.newURI(url);
}

/*
 * Convert a URL (string) into a nsIURI or NSIJARURI
 * This is intended for URL's that are on a file system
 * or in packaged up in an extension .jar file
 *
 * url: a string of a url on the local system(http://localhost/blah.html)
 */
function getResolvedURI(url) {
  var chromeURI = getChromeURI(url);
  var resolvedURI = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry)
    .convertChromeURL(chromeURI);

  try {
    resolvedURI = resolvedURI.QueryInterface(Ci.nsIJARURI);
  } catch (ex) {} // not a jar file

  return resolvedURI;
}

/**
 *  getChromeDir is intended to be called after getResolvedURI and convert
 *  the input URI into a nsIFile (actually the directory containing the
 *  file).  This can be used for copying or referencing the file or extra files
 *  required by the test.  Usually we need to load a secondary html file or library
 *  and this will give us file system access to that.
 *
 *  resolvedURI: nsIURI (from getResolvedURI) that points to a file:/// url
 */
function getChromeDir(resolvedURI) {
  var fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"].getService(
    Ci.nsIFileProtocolHandler
  );
  var chromeDir = fileHandler.getFileFromURLSpec(resolvedURI.spec);
  return chromeDir.parent.QueryInterface(Ci.nsIFile);
}

// used by tests to determine their directory based off window.location.path
function getRootDirectory(path, chromeURI) {
  if (chromeURI === undefined) {
    chromeURI = getChromeURI(path);
  }
  var myURL = chromeURI.QueryInterface(Ci.nsIURL);
  var mydir = myURL.directory;

  if (mydir.match("/$") != "/") {
    mydir += "/";
  }

  return chromeURI.prePath + mydir;
}

// used by tests to determine their directory based off window.location.path
function getChromePrePath(path, chromeURI) {
  if (chromeURI === undefined) {
    chromeURI = getChromeURI(path);
  }

  return chromeURI.prePath;
}

/*
 * Given a URI, return nsIJARURI or null
 */
function getJar(uri) {
  var resolvedURI = getResolvedURI(uri);
  var jar = null;
  try {
    if (resolvedURI.JARFile) {
      jar = resolvedURI;
    }
  } catch (ex) {}
  return jar;
}

/*
 * input:
 *  jar: a nsIJARURI object with the jarfile and jarentry (path in jar file)
 *
 * output;
 *  all files and subdirectories inside jarentry will be extracted to TmpD/mochikit.tmp
 *  we will return the location of /TmpD/mochikit.tmp* so you can reference the files locally
 */
function extractJarToTmp(jar) {
  var tmpdir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  tmpdir.append("mochikit.tmp");
  // parseInt is used because octal escape sequences cause deprecation warnings
  // in strict mode (which is turned on in debug builds)
  tmpdir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));

  var zReader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );

  var fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"].getService(
    Ci.nsIFileProtocolHandler
  );

  var fileName = fileHandler.getFileFromURLSpec(jar.JARFile.spec);
  zReader.open(fileName);

  // filepath represents the path in the jar file without the filename
  var filepath = "";
  var parts = jar.JAREntry.split("/");
  for (var i = 0; i < parts.length - 1; i++) {
    if (parts[i] != "") {
      filepath += parts[i] + "/";
    }
  }

  /* Create dir structure first, no guarantee about ordering of directories and
   * files returned from findEntries.
   */
  for (let dir of zReader.findEntries(filepath + "*/")) {
    var targetDir = buildRelativePath(dir, tmpdir, filepath);
    // parseInt is used because octal escape sequences cause deprecation warnings
    // in strict mode (which is turned on in debug builds)
    if (!targetDir.exists()) {
      targetDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));
    }
  }

  // now do the files
  for (var fname of zReader.findEntries(filepath + "*")) {
    if (fname.substr(-1) != "/") {
      var targetFile = buildRelativePath(fname, tmpdir, filepath);
      zReader.extract(fname, targetFile);
    }
  }
  return tmpdir;
}

/*
 * Take a relative path from the current mochitest file
 * and returns the absolute path for the given test data file.
 */
function getTestFilePath(path) {
  if (path[0] == "/") {
    throw new Error("getTestFilePath only accepts relative path");
  }
  // Get the chrome/jar uri for the current mochitest file
  // gTestPath being defined by the test harness in browser-chrome tests
  // or window is being used for mochitest-browser
  var baseURI = typeof gTestPath == "string" ? gTestPath : window.location.href;
  var parentURI = getResolvedURI(getRootDirectory(baseURI));
  var file;
  if (parentURI.JARFile) {
    // If it's a jar/zip, we have to extract it first
    file = extractJarToTmp(parentURI);
  } else {
    // Otherwise, we can directly cast it to a file URI
    var fileHandler = Cc[
      "@mozilla.org/network/protocol;1?name=file"
    ].getService(Ci.nsIFileProtocolHandler);
    file = fileHandler.getFileFromURLSpec(parentURI.spec);
  }
  // Then walk by the given relative path
  path.split("/").forEach(function(p) {
    if (p == "..") {
      file = file.parent;
    } else if (p != ".") {
      file.append(p);
    }
  });
  return file.path;
}

/*
 * Simple utility function to take the directory structure in jarentryname and
 * translate that to a path of a nsIFile.
 */
function buildRelativePath(jarentryname, destdir, basepath) {
  var baseParts = basepath.split("/");
  if (baseParts[baseParts.length - 1] == "") {
    baseParts.pop();
  }

  var parts = jarentryname.split("/");

  var targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  targetFile.initWithFile(destdir);

  for (var i = baseParts.length; i < parts.length; i++) {
    targetFile.append(parts[i]);
  }

  return targetFile;
}

function readConfig(filename) {
  filename = filename || "testConfig.js";

  var configFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  configFile.append(filename);

  if (!configFile.exists()) {
    return {};
  }

  var fileInStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileInStream.init(configFile, -1, 0, 0);

  var str = NetUtil.readInputStreamToString(
    fileInStream,
    fileInStream.available()
  );
  fileInStream.close();
  return JSON.parse(str);
}

function getTestList(params, callback) {
  var baseurl = "chrome://mochitests/content";
  if (window.parseQueryString) {
    params = window.parseQueryString(location.search.substring(1), true);
  }
  if (!params.baseurl) {
    params.baseurl = baseurl;
  }

  var config = readConfig();
  for (var p in params) {
    if (params[p] == 1) {
      config[p] = true;
    } else if (params[p] == 0) {
      config[p] = false;
    } else {
      config[p] = params[p];
    }
  }
  params = config;
  getTestManifest(
    "http://mochi.test:8888/" + params.manifestFile,
    params,
    callback
  );
}
