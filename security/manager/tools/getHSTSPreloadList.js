/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell \
//                                  [path to]/getHSTSPreloadlist.js \
//                                  [absolute path to]/nsSTSPreloadlist.inc'
// Note: Running this file outputs a new nsSTSPreloadlist.inc in the current
//       working directory.

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource:///modules/XPCOMUtils.jsm");

const SOURCE = "https://chromium.googlesource.com/chromium/src/net/+/master/http/transport_security_state_static.json?format=TEXT";
const OUTPUT = "nsSTSPreloadList.inc";
const ERROR_OUTPUT = "nsSTSPreloadList.errors";
const MINIMUM_REQUIRED_MAX_AGE = 60 * 60 * 24 * 7 * 18;
const MAX_CONCURRENT_REQUESTS = 5;
const MAX_RETRIES = 3;
const REQUEST_TIMEOUT = 30 * 1000;
const ERROR_NONE = "no error";
const ERROR_CONNECTING_TO_HOST = "could not connect to host";
const ERROR_NO_HSTS_HEADER = "did not receive HSTS header";
const ERROR_MAX_AGE_TOO_LOW = "max-age too low: ";
const HEADER = "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"\n" +
"/*****************************************************************************/\n" +
"/* This is an automatically generated file. If you're not                    */\n" +
"/* nsSiteSecurityService.cpp, you shouldn't be #including it.     */\n" +
"/*****************************************************************************/\n" +
"\n" +
"#include <stdint.h>\n";

function download() {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", SOURCE, false); // doing the request synchronously
  try {
    req.send();
  }
  catch (e) {
    throw new Error(`ERROR: problem downloading '${SOURCE}': ${e}`);
  }

  if (req.status != 200) {
    throw new Error("ERROR: problem downloading '" + SOURCE + "': status " +
                    req.status);
  }

  var resultDecoded;
  try {
    resultDecoded = atob(req.responseText);
  }
  catch (e) {
    throw new Error("ERROR: could not decode data as base64 from '" + SOURCE +
                    "': " + e);
  }

  // we have to filter out '//' comments, while not mangling the json
  var result = resultDecoded.replace(/^(\s*)?\/\/[^\n]*\n/mg, "");
  var data = null;
  try {
    data = JSON.parse(result);
  }
  catch (e) {
    throw new Error(`ERROR: could not parse data from '${SOURCE}': ${e}`);
  }
  return data;
}

function getHosts(rawdata) {
  var hosts = [];

  if (!rawdata || !rawdata.entries) {
    throw new Error("ERROR: source data not formatted correctly: 'entries' " +
                    "not found");
  }

  for (let entry of rawdata.entries) {
    if (entry.mode && entry.mode == "force-https") {
      if (entry.name) {
        // We trim the entry name here to avoid malformed URI exceptions when we
        // later try to connect to the domain.
        entry.name = entry.name.trim();
        entry.retries = MAX_RETRIES;
        entry.originalIncludeSubdomains = entry.include_subdomains;
        hosts.push(entry);
      } else {
        throw new Error("ERROR: entry not formatted correctly: no name found");
      }
    }
  }

  return hosts;
}

var gSSService = Cc["@mozilla.org/ssservice;1"]
                   .getService(Ci.nsISiteSecurityService);

function processStsHeader(host, header, status, securityInfo) {
  var maxAge = { value: 0 };
  var includeSubdomains = { value: false };
  var error = ERROR_NONE;
  if (header != null && securityInfo != null) {
    try {
      var uri = Services.io.newURI("https://" + host.name, null, null);
      var sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                  .SSLStatus;
      gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS,
                               uri, header, sslStatus, 0, maxAge,
                               includeSubdomains);
    }
    catch (e) {
      dump("ERROR: could not process header '" + header + "' from " +
           host.name + ": " + e + "\n");
      error = e;
    }
  } else if (status == 0) {
    error = ERROR_CONNECTING_TO_HOST;
  } else {
    error = ERROR_NO_HSTS_HEADER;
  }

  let forceInclude = (host.forceInclude || host.pins == "google");

  if (error == ERROR_NONE && maxAge.value < MINIMUM_REQUIRED_MAX_AGE) {
    error = ERROR_MAX_AGE_TOO_LOW;
  }

  return { name: host.name,
           maxAge: maxAge.value,
           includeSubdomains: includeSubdomains.value,
           error: error,
           retries: host.retries - 1,
           forceInclude: forceInclude,
           originalIncludeSubdomains: host.originalIncludeSubdomains };
}

// RedirectAndAuthStopper prevents redirects and HTTP authentication
function RedirectAndAuthStopper() {}

RedirectAndAuthStopper.prototype = {
  // nsIChannelEventSink
  asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
    throw new Error(Cr.NS_ERROR_ENTITY_CHANGED);
  },

  // nsIAuthPrompt2
  promptAuth: function(channel, level, authInfo) {
    return false;
  },

  asyncPromptAuth: function(channel, callback, context, level, authInfo) {
    throw new Error(Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink,
                                         Ci.nsIAuthPrompt2])
};

function getHSTSStatus(host, resultList) {
  var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  var inResultList = false;
  var uri = "https://" + host.name + "/";
  req.open("GET", uri, true);
  req.timeout = REQUEST_TIMEOUT;
  req.channel.notificationCallbacks = new RedirectAndAuthStopper();
  req.onreadystatechange = function(event) {
    if (!inResultList && req.readyState == 4) {
      inResultList = true;
      var header = req.getResponseHeader("strict-transport-security");
      resultList.push(processStsHeader(host, header, req.status,
                                       req.channel.securityInfo));
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
  if (a.name > b.name) {
    return 1;
  }
  if (a.name < b.name) {
    return -1;
  }
  return 0;
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

function errorToString(status) {
  return (status.error == ERROR_MAX_AGE_TOO_LOW
          ? status.error + status.maxAge
          : status.error);
}

function writeEntry(status, indices, outputStream) {
  let includeSubdomains = (status.finalIncludeSubdomains ? "true" : "false");
  writeTo("  { " + indices[status.name] + ", " + includeSubdomains + " },\n",
          outputStream);
}

function output(sortedStatuses, currentList) {
  try {
    var file = FileUtils.getFile("CurWorkD", [OUTPUT]);
    var errorFile = FileUtils.getFile("CurWorkD", [ERROR_OUTPUT]);
    var fos = FileUtils.openSafeFileOutputStream(file);
    var eos = FileUtils.openSafeFileOutputStream(errorFile);
    writeTo(HEADER, fos);
    writeTo(getExpirationTimeString(), fos);

    for (let status in sortedStatuses) {
      // If we've encountered an error for this entry (other than the site not
      // sending an HSTS header), be safe and don't remove it from the list
      // (given that it was already on the list).
      if (status.error != ERROR_NONE &&
          status.error != ERROR_NO_HSTS_HEADER &&
          status.error != ERROR_MAX_AGE_TOO_LOW &&
          status.name in currentList) {
        dump("INFO: error connecting to or processing " + status.name + " - using previous status on list\n");
        writeTo(status.name + ": " + errorToString(status) + "\n", eos);
        status.maxAge = MINIMUM_REQUIRED_MAX_AGE;
        status.includeSubdomains = currentList[status.name];
      }
    }

    // Filter out entries we aren't including.
    var includedStatuses = sortedStatuses.filter(function (status) {
      if (status.maxAge < MINIMUM_REQUIRED_MAX_AGE && !status.forceInclude) {
        dump("INFO: " + status.name + " NOT ON the preload list\n");
        writeTo(status.name + ": " + errorToString(status) + "\n", eos);
        return false;
      }

      dump("INFO: " + status.name + " ON the preload list\n");
      if (status.forceInclude && status.error != ERROR_NONE) {
        writeTo(status.name + ": " + errorToString(status) + " (error "
                + "ignored - included regardless)\n", eos);
      }
      return true;
    });

    // Resolve whether we should include subdomains for each entry.  We could
    // do this while writing out entries, but separating out that decision is
    // clearer.  Making that decision here also means we can write the choices
    // in the comments in the static string table, which makes parsing the
    // current list significantly easier when we go to update the list.
    for (let status of includedStatuses) {
      let incSubdomainsBool = (status.forceInclude && status.error != ERROR_NONE
                               ? status.originalIncludeSubdomains
                               : status.includeSubdomains);
      status.finalIncludeSubdomains = incSubdomainsBool;
    }

    writeTo("\nstatic const char kSTSHostTable[] = {\n", fos);
    var indices = {};
    var currentIndex = 0;
    for (let status of includedStatuses) {
      indices[status.name] = currentIndex;
      // Add 1 for the null terminator in C.
      currentIndex += status.name.length + 1;
      // Rebuilding the preload list requires reading the previous preload
      // list.  Write out a comment describing each host prior to writing out
      // the string for the host.
      writeTo("  /* \"" + status.name + "\", " +
              (status.finalIncludeSubdomains ? "true" : "false") + " */ ",
              fos);
      // Write out the string itself as individual characters, including the
      // null terminator.  We do it this way rather than using C's string
      // concatentation because some compilers have hardcoded limits on the
      // lengths of string literals, and the preload list is large enough
      // that it runs into said limits.
      for (let c of status.name) {
	writeTo("'" + c + "', ", fos);
      }
      writeTo("'\\0',\n", fos);
    }
    writeTo("};\n", fos);

    const PREFIX = "\n" +
      "struct nsSTSPreload\n" +
      "{\n" +
      "  const uint32_t mHostIndex : 31;\n" +
      "  const uint32_t mIncludeSubdomains : 1;\n" +
      "};\n" +
      "\n" +
      "static const nsSTSPreload kSTSPreloadList[] = {\n";
    const POSTFIX = "};\n";

    writeTo(PREFIX, fos);
    for (let status of includedStatuses) {
      writeEntry(status, indices, fos);
    }
    writeTo(POSTFIX, fos);
    FileUtils.closeSafeFileOutputStream(fos);
    FileUtils.closeSafeFileOutputStream(eos);
  }
  catch (e) {
    dump("ERROR: problem writing output to '" + OUTPUT + "': " + e + "\n");
  }
}

function shouldRetry(response) {
  return (response.error != ERROR_NO_HSTS_HEADER &&
          response.error != ERROR_MAX_AGE_TOO_LOW &&
          response.error != ERROR_NONE && response.retries > 0);
}

function getHSTSStatuses(inHosts, outStatuses) {
  var expectedOutputLength = inHosts.length;
  var tmpOutput = [];
  for (var i = 0; i < MAX_CONCURRENT_REQUESTS && inHosts.length > 0; i++) {
    let host = inHosts.shift();
    dump("spinning off request to '" + host.name + "' (remaining retries: " +
         host.retries + ")\n");
    getHSTSStatus(host, tmpOutput);
  }

  while (outStatuses.length != expectedOutputLength) {
    waitForAResponse(tmpOutput);
    var response = tmpOutput.shift();
    dump("request to '" + response.name + "' finished\n");
    if (shouldRetry(response)) {
      inHosts.push(response);
    } else {
      outStatuses.push(response);
    }

    if (inHosts.length > 0) {
      let host = inHosts.shift();
      dump("spinning off request to '" + host.name + "' (remaining retries: " +
           host.retries + ")\n");
      getHSTSStatus(host, tmpOutput);
    }
  }
}

// Since all events are processed on the main thread, and since event
// handlers are not preemptible, there shouldn't be any concurrency issues.
function waitForAResponse(outputList) {
  // From <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
  var threadManager = Cc["@mozilla.org/thread-manager;1"]
                      .getService(Ci.nsIThreadManager);
  var mainThread = threadManager.currentThread;
  while (outputList.length == 0) {
    mainThread.processNextEvent(true);
  }
}

function readCurrentList(filename) {
  var currentHosts = {};
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(filename);
  var fis = Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(Ci.nsILineInputStream);
  fis.init(file, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  var line = {};
  // While we generate entries matching the version 2 format (see bug 1255425
  // for details), we still need to be able to read entries in the version 1
  // format for bootstrapping a version 2 preload list from a version 1
  // preload list.  Hence these two regexes.
  var v1EntryRegex = /  { "([^"]*)", (true|false) },/;
  var v2EntryRegex = /  \/\* "([^"]*)", (true|false) \*\//;
  while (fis.readLine(line)) {
    var match = v1EntryRegex.exec(line.value);
    if (!match) {
      match = v2EntryRegex.exec(line.value);
    }
    if (match) {
      currentHosts[match[1]] = (match[2] == "true");
    }
  }
  return currentHosts;
}

function combineLists(newHosts, currentHosts) {
  for (let currentHost in currentHosts) {
    let found = false;
    for (let newHost of newHosts) {
      if (newHost.name == currentHost) {
        found = true;
        break;
      }
    }
    if (!found) {
      newHosts.push({ name: currentHost, retries: MAX_RETRIES });
    }
  }
}

// ****************************************************************************
// This is where the action happens:
if (arguments.length != 1) {
  throw new Error("Usage: getHSTSPreloadList.js " +
                  "<absolute path to current nsSTSPreloadList.inc>");
}
// get the current preload list
var currentHosts = readCurrentList(arguments[0]);
// disable the current preload list so it won't interfere with requests we make
Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", false);
// download and parse the raw json file from the Chromium source
var rawdata = download();
// get just the hosts with mode: "force-https"
var hosts = getHosts(rawdata);
// add hosts in the current list to the new list (avoiding duplicates)
combineLists(hosts, currentHosts);
// get the HSTS status of each host
var hstsStatuses = [];
getHSTSStatuses(hosts, hstsStatuses);
// sort the hosts alphabetically
hstsStatuses.sort(compareHSTSStatus);
// write the results to a file (this is where we filter out hosts that we
// either couldn't connect to, didn't receive an HSTS header from, couldn't
// parse the header, or had a header with too short a max-age)
output(hstsStatuses, currentHosts);
// ****************************************************************************
