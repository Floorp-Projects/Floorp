/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell \
//                                  [path to]/genIntolerantFallbackList.js \
//                                  [absolute path to]/IntolerantFallbackList.inc

var { 'classes': Cc, 'interfaces': Ci, 'utils': Cu, 'results': Cr } = Components;

const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

const SEC_ERROR_UNKNOWN_ISSUER = 0x805a1ff3;

const OUTPUT = "IntolerantFallbackList.inc";
const ERROR_OUTPUT = "IntolerantFallbackList.errors";
const MAX_CONCURRENT_REQUESTS = 5;
const MAX_RETRIES = 3;
const REQUEST_TIMEOUT = 30 * 1000;
const TEST_DOMAINS = {
  "fallback.test": true,
};

const FILE_HEADER = `/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///////////////////////////////////////////////////////////////////////////////
// This is an automatically generated file. If you're not
// nsNSSIOLayer.cpp, you shouldn't be #including it.
///////////////////////////////////////////////////////////////////////////////

static const char* const kIntolerantFallbackList[] =
{
`;

const FILE_FOOTER = "};\n";

var errorTable = {};
var errorWithoutFallbacks = {};

try {

  if (arguments.length != 1) {
    throw "Usage: genIntolerantFallbackList.js " +
          "<absolute path to IntolerantFallbackList.inc> ";
  }

  // initialize the error message table
  for (let name in Cr) {
    errorTable[Cr[name]] = name;
  }

  // disable the current fallback list so it won't interfere with requests we make
  Services.prefs.setBoolPref("security.tls.insecure_fallback_hosts.use_static_list", false);

  // get the current preload list
  let gIntolerantFallbackList = readCurrentList(arguments[0]);

  // get the fallback status of each host without whitelist
  let fallbackStatuses = [];
  getFallbackStatuses(gIntolerantFallbackList, fallbackStatuses);

  // reenable the current fallback list
  Services.prefs.clearUserPref("security.tls.insecure_fallback_hosts.use_static_list");

  // get the fallback status of each host with whitelist
  for (let entry of fallbackStatuses) {
    entry.errorWithoutFallback = entry.error;
    delete entry.error;
    entry.retries = MAX_RETRIES;
  }
  gIntolerantFallbackList = fallbackStatuses;
  fallbackStatuses = [];
  getFallbackStatuses(gIntolerantFallbackList, fallbackStatuses);

  // sort the hosts alphabetically
  fallbackStatuses.sort(function(a, b) {
    return (a.name > b.name ? 1 : (a.name < b.name ? -1 : 0));
  });

  // write the results to a file (this is where we filter out hosts that we
  // either could connect to without list, or couldn't connect to with list)
  output(fallbackStatuses);

} catch (e) {
  dump([e, e.stack].join("\n"));
  throw e;
}

function readCurrentList(filename) {
  let currentHosts = [];
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(filename);
  let fis = Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(Ci.nsILineInputStream);
  fis.init(file, -1, -1, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  let line = {};
  let entryRegex = /  "([^"]*)",(?: \/\/ (.*))?/;
  while (fis.readLine(line)) {
    let match = entryRegex.exec(line.value);
    if (match) {
      currentHosts.push({
        name: match[1],
        comment: match[2],
        retries: MAX_RETRIES}
      );
    }
  }
  return currentHosts;
}

function getFallbackStatuses(inHosts, outStatuses) {
  const expectedOutputLength = inHosts.length;
  let tmpOutput = [];
  function handleOneHost() {
    let host = inHosts.shift();
    dump("spinning off request to '" + host.name + "' (remaining retries: " +
         host.retries + ")\n");
    getFallbackStatus(host, tmpOutput);
  }

  for (let i = 0; i < MAX_CONCURRENT_REQUESTS && inHosts.length > 0; i++) {
    handleOneHost();
  }
  while (outStatuses.length != expectedOutputLength) {
    waitForAResponse(tmpOutput);
    let response = tmpOutput.shift();
    dump("request to '" + response.name + "' finished: " +
         errorToString(response.error) + "\n");
    outStatuses.push(response);

    if (inHosts.length > 0) {
      handleOneHost();
    }
  }
}

function getFallbackStatus(host, resultList) {
  if (TEST_DOMAINS[host.name]) {
    host.error = Cr.NS_OK;
    host.retries--;
    resultList.push(host);
    return;
  }
  let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
            .createInstance(Ci.nsIXMLHttpRequest);
  let inResultList = false;
  let uri = "https://" + host.name + "/";
  req.open("GET", uri, true);
  req.timeout = REQUEST_TIMEOUT;
  req.channel.notificationCallbacks = new RedirectAndAuthStopper();
  req.onreadystatechange = function(event) {
    if (!inResultList && req.readyState == 4) {
      inResultList = true;
      // If the url is changed, a redirect happened that means
      // a successfull TLS handshake. Treat it as success rather than
      // using a status from the redirect target.
      host.error = uri == req.responseURL ? req.channel.status : Cr.NS_OK;
      host.retries--;
      resultList.push(host);
    }
  };

  try {
    req.send();
  }
  catch (e) {
    dump("ERROR: exception making request to " + host.name + ": " + e + "\n");
  }
}

// RedirectAndAuthStopper prevents redirects and HTTP authentication
function RedirectAndAuthStopper() {};

RedirectAndAuthStopper.prototype = {
  // nsIChannelEventSink
  asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
    throw Cr.NS_ERROR_ENTITY_CHANGED;
  },

  // nsIAuthPrompt2
  promptAuth: function(channel, level, authInfo) {
    return false;
  },

  asyncPromptAuth: function(channel, callback, context, level, authInfo) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink,
                                         Ci.nsIAuthPrompt2])
};

// Since all events are processed on the main thread, and since event
// handlers are not preemptible, there shouldn't be any concurrency issues.
function waitForAResponse(outputList) {
  // From <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
  let threadManager = Cc["@mozilla.org/thread-manager;1"]
                      .getService(Ci.nsIThreadManager);
  let mainThread = threadManager.currentThread;
  while (outputList.length == 0) {
    mainThread.processNextEvent(true);
  }
}

function errorToString(error) {
  return error != undefined ? errorTable[error] || ("0x" + error.toString(16)) : error;
}

function formatComment(comment) {
  return comment ? " // " + comment : "";
}

function success(error) {
  return error == Cr.NS_OK || error == SEC_ERROR_UNKNOWN_ISSUER;
}

function output(sortedStatuses) {
  let file = FileUtils.getFile("CurWorkD", [OUTPUT]);
  let errorFile = FileUtils.getFile("CurWorkD", [ERROR_OUTPUT]);
  let fos = FileUtils.openSafeFileOutputStream(file);
  let eos = FileUtils.openSafeFileOutputStream(errorFile);
  writeTo(FILE_HEADER, fos);
  for (let status of sortedStatuses) {

    if (!TEST_DOMAINS[status.name] &&
        success(status.errorWithoutFallback)) {
      dump("INFO: " + status.name + " does NOT require fallback\n");
      writeTo(status.name + ": worked (" +
              errorToString(status.errorWithoutFallback) + " / " +
              errorToString(status.error) + ")" +
              formatComment(status.comment) + "\n", eos);
    } else {
      if (!success(status.error)) {
        dump("INFO: " + status.name + " is dead?\n");
        writeTo(status.name + ": failed (" +
                errorToString(status.errorWithoutFallback) + " / " +
                errorToString(status.error) + ")" +
                formatComment(status.comment) + "\n", eos);
      }
      dump("INFO: " + status.name + " ON the fallback list\n");
      writeTo("  \"" + status.name + "\"," +
              formatComment(status.comment) + "\n", fos);
    }
  }
  writeTo(FILE_FOOTER, fos);
  FileUtils.closeSafeFileOutputStream(fos);
  FileUtils.closeSafeFileOutputStream(eos);
}

function writeTo(string, fos) {
  fos.write(string, string.length);
}
