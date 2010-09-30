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
 * The Original Code is Mochitest code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joel Maher <joel.maher@gmail.com>
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
 * getChromeURI converts a URL to a URI
 * 
 * url: string of a URL (http://mochi.test/test.html)
 * returns: a nsiURI object representing the given URL
 *
 */
function getChromeURI(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
              getService(Components.interfaces.nsIIOService);
  return ios.newURI(url, null, null);
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
  var resolvedURI = Components.classes["@mozilla.org/chrome/chrome-registry;1"].
                    getService(Components.interfaces.nsIChromeRegistry).
                    convertChromeURL(chromeURI);

  try {
    resolvedURI = resolvedURI.QueryInterface(Components.interfaces.nsIJARURI);
  } catch (ex) {} //not a jar file

  return resolvedURI;
}

/**
 *  getChromeDir is intended to be called after getResolvedURI and convert
 *  the input URI into a nsILocalFile (actually the directory containing the
 *  file).  This can be used for copying or referencing the file or extra files
 *  required by the test.  Usually we need to load a secondary html file or library
 *  and this will give us file system access to that.
 *
 *  resolvedURI: nsIURI (from getResolvedURI) that points to a file:/// url
 */
function getChromeDir(resolvedURI) {

  var fileHandler = Components.classes["@mozilla.org/network/protocol;1?name=file"].
                    getService(Components.interfaces.nsIFileProtocolHandler);
  var chromeDir = fileHandler.getFileFromURLSpec(resolvedURI.spec);
  return chromeDir.parent.QueryInterface(Components.interfaces.nsILocalFile);
}

/*
 * given a .jar file, we get all test files located inside the archive
 *
 * basePath: base URL to determine chrome location and search for tests
 * testPath: passed in testPath value from command line such as: dom/tests/mochitest
 * dir: the test dir to append to the baseURL after getting a directory interface
 *
 * As a note, this is hardcoded to the .jar structure we use for mochitest.  
 * Please don't assume this works for all jar files.
 */
function getMochitestJarListing(basePath, testPath, dir)
{
  var zReader = Components.classes["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Components.interfaces.nsIZipReader);
  var fileHandler = Components.classes["@mozilla.org/network/protocol;1?name=file"].
                    getService(Components.interfaces.nsIFileProtocolHandler);

  var fileName = fileHandler.getFileFromURLSpec(getResolvedURI(basePath).JARFile.spec);
  zReader.open(fileName);
  //hardcoded 'content' as that is the root dir in the mochikit.jar file
  var base = "content/" + dir + "/";

  var singleTestPath;
  if (testPath) {
    var extraPath = testPath;
    var pathToCheck = base + testPath;
    if (zReader.hasEntry(pathToCheck)) {
      var pathEntry = zReader.getEntry(pathToCheck);
      if (pathEntry.isDirectory) {
        base = pathToCheck;
      } else {
        singleTestPath = '/' + base + testPath;
        var singleObject = {};
        singleObject[singleTestPath] = true;
        return [singleObject, singleTestPath];
      }
    }
    else if (zReader.hasEntry(pathToCheck + "/")) {
      base = pathToCheck + "/";
    }
  }
  var [links, count] = zList(base, zReader, true);
  return [links, null];
}

/*
 * Replicate the server.js list() function with a .jar file
 *
 * base: string value of base directory we are testing
 * zReader: handle to opened nsIZipReader object
 * recurse: true|false if we do subdirs
 *
 * returns:
 *  [json object of {dir:{subdir:{file:true, file:true, ...}}}, count of tests]
 */
function zList(base, zReader, recurse) {
  var dirs = zReader.findEntries(base + "*");
  var links = {};
  var count = 0;
  var fileArray = [];
  
  while(dirs.hasMore()) {
    var entryName = dirs.getNext();
    if (entryName.substr(-1) == '/' && entryName.split('/').length == (base.split('/').length + 1) ||
        (entryName.substr(-1) != '/' && entryName.split('/').length == (base.split('/').length))) { 
      fileArray.push(entryName);
    }
  }
  fileArray.sort();
  count = fileArray.length;
  for (var i=0; i < fileArray.length; i++) {
    var myFile = fileArray[i];
    if (myFile.substr(-1) === '/' && recurse) {
      var childCount = 0;
      [links[myFile], childCount] = zList(myFile, zReader, recurse);
      count += childCount;
    } else {
      if (myFile.indexOf("SimpleTest") == -1) {
        //we add the '/' so we don't try to run content/content/chrome
        links['/' + myFile] = true;
      }
    }
  }
  return [links, count];
}

/**
 * basePath: the URL base path to search from such as chrome://mochikit/content/a11y
 * testPath: the optional testPath passed into the test such as dom/tests/mochitest
 * dir: the test dir to append to the uri after getting a directory interface
 * srvScope: loaded javascript to server.js so we have aComponents.classesess to the list() function
 *
 * return value:
 *  single test: [json object, path to test]
 *  list of tests: [json object, null] <- directory [heirarchy]
 */
function getFileListing(basePath, testPath, dir, srvScope)
{
  var uri = getResolvedURI(basePath);
  var chromeDir = getChromeDir(uri);
  chromeDir.appendRelativePath(dir);
  basePath += '/' + dir;

  var ioSvc = Components.classes["@mozilla.org/network/io-service;1"].
              getService(Components.interfaces.nsIIOService);
  var testsDirURI = ioSvc.newFileURI(chromeDir);
  var testsDir = ioSvc.newURI(testPath, null, testsDirURI)
                  .QueryInterface(Components.interfaces.nsIFileURL).file;

  var singleTestPath;
  if (testPath != undefined) {
    var extraPath = testPath;
    
    var fileNameRegexp = /(browser|test)_.+\.(xul|html|js)$/;

    // Invalid testPath...
    if (!testsDir.exists())
      return [];

    // If we were passed a speComponents.interfacesfic file, run only that test.
    if (testsDir.isFile()) {
      if (fileNameRegexp.test(testsDir.leafName))
        var singlePath = basePath + '/' + testPath;
        var links = {};
        links[singlePath] = true;
        return [links, singlePath];

      // We were passed a file that's not a test...
      return [];
    }

    // otherwise, we were passed a directory of tests
    basePath += "/" + testPath;
  }
  var [links, count] = srvScope.list(basePath, testsDir, true);
  return [links, null];
}


//used by tests to determine their directory based off window.location.path
function getRootDirectory(path, chromeURI) {
  if (chromeURI === undefined)
  {
    chromeURI = getChromeURI(path);
  }
  var myURL = chromeURI.QueryInterface(Components.interfaces.nsIURL);
  var mydir = myURL.directory;

  if (mydir.match('/$') != '/')
  {
    mydir += '/';
  }

  return chromeURI.prePath + mydir;
}

//used by tests to determine their directory based off window.location.path
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
  var tmpdir = Components.classes["@mozilla.org/file/directory_service;1"]
                      .getService(Components.interfaces.nsIProperties)
                      .get("ProfD", Components.interfaces.nsILocalFile);
  tmpdir.append("mochikit.tmp");
  // parseInt is used because octal escape sequences cause deprecation warnings
  // in strict mode (which is turned on in debug builds)
  tmpdir.createUnique(Components.interfaces.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));

  var zReader = Components.classes["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Components.interfaces.nsIZipReader);

  var fileHandler = Components.classes["@mozilla.org/network/protocol;1?name=file"].
                    getService(Components.interfaces.nsIFileProtocolHandler);

  var fileName = fileHandler.getFileFromURLSpec(jar.JARFile.spec);
  zReader.open(fileName);

  //filepath represents the path in the jar file without the filename
  var filepath = "";
  var parts = jar.JAREntry.split('/');
  for (var i =0; i < parts.length - 1; i++) {
    if (parts[i] != '') {
      filepath += parts[i] + '/';
    }
  }

  /* Create dir structure first, no guarantee about ordering of directories and
   * files returned from findEntries.
   */
  var dirs = zReader.findEntries(filepath + '*/');
  while (dirs.hasMore()) {
    var targetDir = buildRelativePath(dirs.getNext(), tmpdir, filepath);
    // parseInt is used because octal escape sequences cause deprecation warnings
    // in strict mode (which is turned on in debug builds)
    targetDir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, parseInt("0777", 8));
  }

  //now do the files
  var files = zReader.findEntries(filepath + "*");
  while (files.hasMore()) {
    var fname = files.getNext();
    if (fname.substr(-1) != '/') {
      var targetFile = buildRelativePath(fname, tmpdir, filepath);
      zReader.extract(fname, targetFile);
    }
  }
  return tmpdir;
}

/*
 * Simple utility function to take the directory structure in jarentryname and 
 * translate that to a path of a nsILocalFile.
 */
function buildRelativePath(jarentryname, destdir, basepath)
{
  var baseParts = basepath.split('/');
  if (baseParts[baseParts.length-1] == '') {
    baseParts.pop();
  }

  var parts = jarentryname.split('/');

  var targetFile = Components.classes["@mozilla.org/file/local;1"]
                   .createInstance(Components.interfaces.nsILocalFile);
  targetFile.initWithFile(destdir);

  for (var i = baseParts.length; i < parts.length; i++) {
    targetFile.append(parts[i]);
  }

  return targetFile;
}

