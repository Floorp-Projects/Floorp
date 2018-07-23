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

var gSSService = Cc["@mozilla.org/ssservice;1"]
                   .getService(Ci.nsISiteSecurityService);

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

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
const GPERF_DELIM = "%%\n";

function download() {
  let req = new XMLHttpRequest();
  req.open("GET", SOURCE, false); // doing the request synchronously
  try {
    req.send();
  } catch (e) {
    throw new Error(`ERROR: problem downloading '${SOURCE}': ${e}`);
  }

  if (req.status != 200) {
    throw new Error("ERROR: problem downloading '" + SOURCE + "': status " +
                    req.status);
  }

  let resultDecoded;
  try {
    resultDecoded = atob(req.responseText);
  } catch (e) {
    throw new Error("ERROR: could not decode data as base64 from '" + SOURCE +
                    "': " + e);
  }

  // we have to filter out '//' comments, while not mangling the json
  let result = resultDecoded.replace(/^(\s*)?\/\/[^\n]*\n/mg, "");
  let data = null;
  try {
    data = JSON.parse(result);
  } catch (e) {
    throw new Error(`ERROR: could not parse data from '${SOURCE}': ${e}`);
  }
  return data;
}

function getHosts(rawdata) {
  let hosts = [];

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
        // We prefer the camelCase variable to the JSON's snake case version
        entry.includeSubdomains = entry.include_subdomains;
        hosts.push(entry);
      } else {
        throw new Error("ERROR: entry not formatted correctly: no name found");
      }
    }
  }

  return hosts;
}

function processStsHeader(host, header, status, securityInfo) {
  let maxAge = { value: 0 };
  let includeSubdomains = { value: false };
  let error = ERROR_NONE;
  if (header != null && securityInfo != null) {
    try {
      let uri = Services.io.newURI("https://" + host.name);
      let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                  .SSLStatus;
      gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS,
                               uri, header, sslStatus, 0,
                               Ci.nsISiteSecurityService.SOURCE_PRELOAD_LIST,
                               {}, maxAge, includeSubdomains);
    } catch (e) {
      dump("ERROR: could not process header '" + header + "' from " +
           host.name + ": " + e + "\n");
      error = e;
    }
  } else if (status == 0) {
    error = ERROR_CONNECTING_TO_HOST;
  } else {
    error = ERROR_NO_HSTS_HEADER;
  }

  if (error == ERROR_NONE && maxAge.value < MINIMUM_REQUIRED_MAX_AGE) {
    error = ERROR_MAX_AGE_TOO_LOW;
  }

  return { name: host.name,
           maxAge: maxAge.value,
           includeSubdomains: includeSubdomains.value,
           error,
           retries: host.retries - 1,
           forceInclude: host.forceInclude };
}

// RedirectAndAuthStopper prevents redirects and HTTP authentication
function RedirectAndAuthStopper() {}

RedirectAndAuthStopper.prototype = {
  // nsIChannelEventSink
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    throw new Error(Cr.NS_ERROR_ENTITY_CHANGED);
  },

  // nsIAuthPrompt2
  promptAuth(channel, level, authInfo) {
    return false;
  },

  asyncPromptAuth(channel, callback, context, level, authInfo) {
    throw new Error(Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  getInterface(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIChannelEventSink,
                                          Ci.nsIAuthPrompt2])
};

function getHSTSStatus(host, resultList) {
  let req = new XMLHttpRequest();
  let inResultList = false;
  let uri = "https://" + host.name + "/";
  req.open("GET", uri, true);
  req.setRequestHeader("X-Automated-Tool",
                       "https://hg.mozilla.org/mozilla-central/file/tip/security/manager/tools/getHSTSPreloadList.js");
  req.timeout = REQUEST_TIMEOUT;

  let errorhandler = (evt) => {
    dump(`ERROR: error making request to ${host.name} (type=${evt.type})\n`);
    if (!inResultList) {
      inResultList = true;
      resultList.push(processStsHeader(host, null, req.status,
                                       req.channel.securityInfo));
    }
  };
  req.onerror = errorhandler;
  req.ontimeout = errorhandler;
  req.onabort = errorhandler;

  req.onload = function(event) {
    if (!inResultList) {
      inResultList = true;
      var header = req.getResponseHeader("strict-transport-security");
      resultList.push(processStsHeader(host, header, req.status,
                                       req.channel.securityInfo));
    }
  };

  try {
    req.channel.notificationCallbacks = new RedirectAndAuthStopper();
    req.send();
  } catch (e) {
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
  let now = new Date();
  let nowMillis = now.getTime();
  // MINIMUM_REQUIRED_MAX_AGE is in seconds, so convert to milliseconds
  let expirationMillis = nowMillis + (MINIMUM_REQUIRED_MAX_AGE * 1000);
  let expirationMicros = expirationMillis * 1000;
  return "const PRTime gPreloadListExpirationTime = INT64_C(" + expirationMicros + ");\n";
}

function errorToString(status) {
  return (status.error == ERROR_MAX_AGE_TOO_LOW
          ? status.error + status.maxAge
          : status.error);
}

function output(sortedStatuses, currentList) {
  try {
    let file = FileUtils.getFile("CurWorkD", [OUTPUT]);
    let errorFile = FileUtils.getFile("CurWorkD", [ERROR_OUTPUT]);
    let fos = FileUtils.openSafeFileOutputStream(file);
    let eos = FileUtils.openSafeFileOutputStream(errorFile);
    writeTo(HEADER, fos);
    writeTo(getExpirationTimeString(), fos);

    for (let status of sortedStatuses) {
      // If we've encountered an error for this entry (other than the site not
      // sending an HSTS header), be safe and don't remove it from the list
      // (given that it was already on the list).
      if (!status.forceInclude &&
          status.error != ERROR_NONE &&
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

      dump("INFO: " + status.name + " ON the preload list (includeSubdomains: "
           + status.includeSubdomains + "\n");
      if (status.forceInclude && status.error != ERROR_NONE) {
        writeTo(status.name + ": " + errorToString(status) + " (error "
                + "ignored - included regardless)\n", eos);
      }
      return true;
    });

    writeTo(GPERF_DELIM, fos);

    for (let status of includedStatuses) {
      let includeSubdomains = (status.includeSubdomains ? 1 : 0);
      writeTo(status.name + ", " + includeSubdomains + "\n", fos);
    }

    writeTo(GPERF_DELIM, fos);
    FileUtils.closeSafeFileOutputStream(fos);
    FileUtils.closeSafeFileOutputStream(eos);
  } catch (e) {
    dump("ERROR: problem writing output to '" + OUTPUT + "': " + e + "\n");
  }
}

function shouldRetry(response) {
  return (response.error != ERROR_NO_HSTS_HEADER &&
          response.error != ERROR_MAX_AGE_TOO_LOW &&
          response.error != ERROR_NONE && response.retries > 0);
}

function probeHSTSStatuses(inHosts) {
  let outStatuses = [];
  let expectedOutputLength = inHosts.length;
  let tmpOutput = [];
  for (let i = 0; i < MAX_CONCURRENT_REQUESTS && inHosts.length > 0; i++) {
    let host = inHosts.shift();
    dump("spinning off request to '" + host.name + "' (remaining retries: " +
         host.retries + ")\n");
    getHSTSStatus(host, tmpOutput);
  }

  while (outStatuses.length != expectedOutputLength) {
    waitForAResponse(tmpOutput);
    let response = tmpOutput.shift();
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

  return outStatuses;
}

// Since all events are processed on the main thread, and since event
// handlers are not preemptible, there shouldn't be any concurrency issues.
function waitForAResponse(outputList) {
  // From <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
  Services.tm.spinEventLoopUntil(() => outputList.length != 0);
}

function readCurrentList(filename) {
  var currentHosts = {};
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(filename);
  var fis = Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(Ci.nsILineInputStream);
  fis.init(file, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  var line = {};

  // While we generate entries matching the latest version format,
  // we still need to be able to read entries in the previous version formats
  // for bootstrapping a latest version preload list from a previous version
  // preload list. Hence these regexes.
  const entryRegexes = [
    /([^,]+), (0|1)/,                         // v3
    / {2}\/\* "([^"]*)", (true|false) \*\//,  // v2
    / {2}{ "([^"]*)", (true|false) },/,       // v1
  ];

  while (fis.readLine(line)) {
    let match;
    entryRegexes.find((r) => { match = r.exec(line.value); return match; });
    if (match) {
      currentHosts[match[1]] = (match[2] == "1" || match[2] == "true");
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

const TEST_ENTRIES = [
  { name: "includesubdomains.preloaded.test", includeSubdomains: true },
  { name: "includesubdomains2.preloaded.test", includeSubdomains: true },
  { name: "noincludesubdomains.preloaded.test", includeSubdomains: false },
];

function deleteTestHosts(currentHosts) {
  for (let testEntry of TEST_ENTRIES) {
    delete currentHosts[testEntry.name];
  }
}

function getTestHosts() {
  let hosts = [];
  for (let testEntry of TEST_ENTRIES) {
    hosts.push({
      name: testEntry.name,
      maxAge: MINIMUM_REQUIRED_MAX_AGE,
      includeSubdomains: testEntry.includeSubdomains,
      error: ERROR_NONE,
      // This deliberately doesn't have a value for `retries` (because we should
      // never attempt to connect to this host).
      forceInclude: true,
    });
  }
  return hosts;
}

function insertHosts(inoutHostList, inAddedHosts) {
  for (let host of inAddedHosts) {
    inoutHostList.push(host);
  }
}

function filterForcedInclusions(inHosts, outNotForced, outForced) {
  // Apply our filters (based on policy today) to determine which entries
  // will be included without being checked (forced); the others will be
  // checked using active probing.
  for (let host of inHosts) {
    if (host.policy == "google" || host.policy == "public-suffix" ||
        host.policy == "public-suffix-requested") {
      host.forceInclude = true;
      host.error = ERROR_NONE;
      outForced.push(host);
    } else {
      outNotForced.push(host);
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
let currentHosts = readCurrentList(arguments[0]);
// delete any hosts we use in tests so we don't actually connect to them
deleteTestHosts(currentHosts);
// disable the current preload list so it won't interfere with requests we make
Services.prefs.setBoolPref("network.stricttransportsecurity.preloadlist", false);
// download and parse the raw json file from the Chromium source
let rawdata = download();
// get just the hosts with mode: "force-https"
let hosts = getHosts(rawdata);
// add hosts in the current list to the new list (avoiding duplicates)
combineLists(hosts, currentHosts);
// Don't contact hosts that are forced to be included anyway
let hostsToContact = [];
let forcedHosts = [];
filterForcedInclusions(hosts, hostsToContact, forcedHosts);
// Initialize the final status list
let hstsStatuses = [];
// Add the hosts we use in tests
insertHosts(hstsStatuses, getTestHosts());
// Add in the hosts that are forced
insertHosts(hstsStatuses, forcedHosts);
// Probe the HSTS status of each host and add them into hstsStatuses
let probedStatuses = probeHSTSStatuses(hostsToContact);
insertHosts(hstsStatuses, probedStatuses);
// Sort the hosts alphabetically
hstsStatuses.sort(compareHSTSStatus);
// Write the results to a file (this is where we filter out hosts that we
// either couldn't connect to, didn't receive an HSTS header from, couldn't
// parse the header, or had a header with too short a max-age)
output(hstsStatuses, currentHosts);
// ****************************************************************************
