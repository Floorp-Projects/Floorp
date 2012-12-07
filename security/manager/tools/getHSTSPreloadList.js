/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
// <https://bugzilla.mozilla.org/show_bug.cgi?id=546628>
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// Register resource://app/ URI
let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
let resHandler = ios.getProtocolHandler("resource")
                 .QueryInterface(Ci.nsIResProtocolHandler);
let mozDir = Cc["@mozilla.org/file/directory_service;1"]
             .getService(Ci.nsIProperties)
             .get("CurProcD", Ci.nsILocalFile);
let mozDirURI = ios.newFileURI(mozDir);
resHandler.setSubstitution("app", mozDirURI);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource:///modules/XPCOMUtils.jsm");

const SOURCE = "https://src.chromium.org/viewvc/chrome/trunk/src/net/base/transport_security_state_static.json";
const OUTPUT = "nsSTSPreloadList.inc";
const ERROR_OUTPUT = "nsSTSPreloadList.errors";
const MINIMUM_REQUIRED_MAX_AGE = 60 * 60 * 24 * 7 * 18;
const HEADER = "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"\n" +
"/*****************************************************************************/\n" +
"/* This is an automatically generated file. If you're not                    */\n" +
"/* nsStrictTransportSecurityService.cpp, you shouldn't be #including it.     */\n" +
"/*****************************************************************************/\n" +
"\n" +
"#include \"mozilla/StandardInteger.h\"\n";
const PREFIX = "\n" +
"class nsSTSPreload\n" +
"{\n" +
"  public:\n" +
"    const char *mHost;\n" +
"    const bool mIncludeSubdomains;\n" +
"};\n" +
"\n" +
"static const nsSTSPreload kSTSPreloadList[] = {\n";
const POSTFIX =  "};\n";

function download() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", SOURCE, false); // doing the request synchronously
  try {
    req.send();
  }
  catch (e) {
    throw "ERROR: problem downloading '" + SOURCE + "': " + e;
  }

  if (req.status != 200) {
    throw "ERROR: problem downloading '" + SOURCE + "': status " + req.status;
  }

  // we have to filter out '//' comments
  var result = req.responseText.replace(/\/\/[^\n]*\n/g, "");
  var data = null;
  try {
    data = JSON.parse(result);
  }
  catch (e) {
    throw "ERROR: could not parse data from '" + SOURCE + "': " + e;
  }
  return data;
}

function getHosts(rawdata) {
  var hosts = [];

  if (!rawdata || !rawdata.entries) {
    throw "ERROR: source data not formatted correctly: 'entries' not found";
  }

  for (entry of rawdata.entries) {
    if (entry.mode && entry.mode == "force-https") {
      if (entry.name) {
        hosts.push(entry);
      } else {
        throw "ERROR: entry not formatted correctly: no name found";
      }
    }
  }

  return hosts;
}

var gSTSService = Cc["@mozilla.org/stsservice;1"]
                  .getService(Ci.nsIStrictTransportSecurityService);

function processStsHeader(hostname, header, status) {
  var maxAge = { value: 0 };
  var includeSubdomains = { value: false };
  var error = "no error";
  if (header != null) {
    try {
      var uri = Services.io.newURI("https://" + host.name, null, null);
      gSTSService.processStsHeader(uri, header, 0, maxAge, includeSubdomains);
    }
    catch (e) {
      dump("ERROR: could not process header '" + header + "' from " + hostname +
           ": " + e + "\n");
      error = e;
    }
  }
  else {
    if (status == 0) {
      error = "could not connect to host";
    } else {
      error = "did not receive HSTS header";
    }
  }

  return { hostname: hostname,
           maxAge: maxAge.value,
           includeSubdomains: includeSubdomains.value,
           error: error };
}

function RedirectStopper() {};

RedirectStopper.prototype = {
  // nsIChannelEventSink
  asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
    throw Cr.NS_ERROR_ENTITY_CHANGED;
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink])
};

function getHSTSStatus(host, resultList) {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  var inResultList = false;
  var uri = "https://" + host.name + "/";
  req.open("GET", uri, true);
  req.channel.notificationCallbacks = new RedirectStopper();
  req.onreadystatechange = function(event) {
    if (!inResultList && req.readyState == 4) {
      inResultList = true;
      var header = req.getResponseHeader("strict-transport-security");
      resultList.push(processStsHeader(host.name, header, req.status));
    }
  };

  try {
    req.send();
  }
  catch (e) {
    dump("ERROR: exception making request to " + host.name + ": " + e + "\n");
  }
}

function compareHSTSStatus(a, b) {
  return (a.hostname > b.hostname ? 1 : (a.hostname < b.hostname ? -1 : 0));
}

function writeTo(string, fos) {
  fos.write(string, string.length);
}

// Determines and returns a string representing a declaration of when this
// preload list should no longer be used.
// This is the current time plus MINIMUM_REQUIRED_MAX_AGE.
function getExpirationTimeString() {
  var now = new Date();
  var nowMillis = now.getTime();
  // MINIMUM_REQUIRED_MAX_AGE is in seconds, so convert to milliseconds
  var expirationMillis = nowMillis + (MINIMUM_REQUIRED_MAX_AGE * 1000);
  var expirationMicros = expirationMillis * 1000;
  return "const PRTime gPreloadListExpirationTime = INT64_C(" + expirationMicros + ");\n";
}

function output(sortedStatuses) {
  try {
    var file = FileUtils.getFile("CurWorkD", [OUTPUT]);
    var errorFile = FileUtils.getFile("CurWorkD", [ERROR_OUTPUT]);
    var fos = FileUtils.openSafeFileOutputStream(file);
    var eos = FileUtils.openSafeFileOutputStream(errorFile);
    writeTo(HEADER, fos);
    writeTo(getExpirationTimeString(), fos);
    writeTo(PREFIX, fos);
    for (var status of hstsStatuses) {
      if (status.maxAge >= MINIMUM_REQUIRED_MAX_AGE) {
        writeTo("  { \"" + status.hostname + "\", " +
                 (status.includeSubdomains ? "true" : "false") + " },\n", fos);
        dump("INFO: " + status.hostname + " ON the preload list\n");
      }
      else {
        dump("INFO: " + status.hostname + " NOT ON the preload list\n");
        if (status.maxAge != 0) {
          status.error = "max-age too low: " + status.maxAge;
        }
        writeTo(status.hostname + ": " + status.error + "\n", eos);
      }
    }
    writeTo(POSTFIX, fos);
    FileUtils.closeSafeFileOutputStream(fos);
    FileUtils.closeSafeFileOutputStream(eos);
  }
  catch (e) {
    dump("ERROR: problem writing output to '" + OUTPUT + "': " + e + "\n");
  }
}

// The idea is the output list will be the same size as the input list
// when we've received all responses (or timed out).
// Since all events are processed on the main thread, and since event
// handlers are not preemptible, there shouldn't be any concurrency issues.
function waitForResponses(inputList, outputList) {
  // From <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
  var threadManager = Cc["@mozilla.org/thread-manager;1"]
                      .getService(Ci.nsIThreadManager);
  var mainThread = threadManager.currentThread;
  while (inputList.length != outputList.length) {
    mainThread.processNextEvent(true);
  }
  while (mainThread.hasPendingEvents()) {
    mainThread.processNextEvent(true);
  }
}

// ****************************************************************************
// This is where the action happens:
// disable the current preload list so it won't interfere with requests we make
Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", false);
// download and parse the raw json file from the Chromium source
var rawdata = download();
// get just the hosts with mode: "force-https"
var hosts = getHosts(rawdata);
// spin off a request to each host
var hstsStatuses = [];
for (var host of hosts) {
  getHSTSStatus(host, hstsStatuses);
}
// wait for those responses to come back
waitForResponses(hosts, hstsStatuses);
// sort the hosts alphabetically
hstsStatuses.sort(compareHSTSStatus);
// write the results to a file (this is where we filter out hosts that we
// either couldn't connect to, didn't receive an HSTS header from, couldn't
// parse the header, or had a header with too short a max-age)
output(hstsStatuses);
// ****************************************************************************
