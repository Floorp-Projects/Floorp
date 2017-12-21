"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

function readTestData(testFile) {
  var testcase = {};

  // Got a feed file, now we need to parse out the Description and Expect headers.
  var istream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
  try {
    istream.init(testFile, 0x01, parseInt("0444", 8), 0);
    istream.QueryInterface(Ci.nsILineInputStream);

    var hasmore = false;
    do {
      var line = {};
      hasmore = istream.readLine(line);

      if (line.value.indexOf("Description:") > -1) {
        testcase.desc = line.value.substring(line.value.indexOf(":") + 1).trim();
      }

      if (line.value.indexOf("Expect:") > -1) {
        testcase.expect = line.value.substring(line.value.indexOf(":") + 1).trim();
      }

      if (line.value.indexOf("Base:") > -1) {
        testcase.base = NetUtil.newURI(line.value.substring(line.value.indexOf(":") + 1).trim());
      }

      if (testcase.expect && testcase.desc) {
        testcase.path = "xml/" + testFile.parent.leafName + "/" + testFile.leafName;
        testcase.file = testFile;
        break;
      }

    } while (hasmore);

  } catch (e) {
    Assert.ok(false, "FAILED! Error reading testFile case in file " + testFile.leafName + " ---- " + e);
  } finally {
    istream.close();
  }

  return testcase;
}

function iterateDir(dir, recurse, callback) {
  info("Iterate " + dir.leafName);
  let entries = dir.directoryEntries;

  // Loop over everything in this dir. If its a dir
  while (entries.hasMoreElements()) {
    let entry = entries.getNext();
    entry.QueryInterface(Ci.nsIFile);

    if (entry.isDirectory()) {
      if (recurse) {
        iterateDir(entry, recurse, callback);
      }
    } else {
      callback(entry);
    }
  }
}

function isIID(a, iid) {
  try {
    a.QueryInterface(iid);
    return true;
  } catch (e) { }

  return false;
}
