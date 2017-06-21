/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head_global.js */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/services/common/logging.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");

function do_check_empty(obj) {
  do_check_attribute_count(obj, 0);
}

function do_check_attribute_count(obj, c) {
  do_check_eq(c, Object.keys(obj).length);
}

function do_check_throws(aFunc, aResult, aStack) {
  if (!aStack) {
    try {
      // We might not have a 'Components' object.
      aStack = Components.stack.caller;
    } catch (e) {}
  }

  try {
    aFunc();
  } catch (e) {
    do_check_eq(e.result, aResult, aStack);
    return;
  }
  do_throw("Expected result " + aResult + ", none thrown.", aStack);
}


/**
 * Test whether specified function throws exception with expected
 * result.
 *
 * @param func
 *        Function to be tested.
 * @param message
 *        Message of expected exception. <code>null</code> for no throws.
 */
function do_check_throws_message(aFunc, aResult) {
  try {
    aFunc();
  } catch (e) {
    do_check_eq(e.message, aResult);
    return;
  }
  do_throw("Expected an error, none thrown.");
}

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
var _ = function(some, debug, text, to) {
  print(Array.slice(arguments).join(" "));
};

function httpd_setup(handlers, port = -1) {
  let server = new HttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  try {
    server.start(port);
  } catch (ex) {
    _("==========================================");
    _("Got exception starting HTTP server on port " + port);
    _("Error: " + Log.exceptionStr(ex));
    _("Is there a process already listening on port " + port + "?");
    _("==========================================");
    do_throw(ex);
  }

  // Set the base URI for convenience.
  let i = server.identity;
  server.baseURI = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;

  return server;
}

function httpd_handler(statusCode, status, body) {
  return function handler(request, response) {
    _("Processing request");
    // Allow test functions to inspect the request.
    request.body = readBytesFromInputStream(request.bodyInputStream);
    handler.request = request;

    response.setStatusLine(request.httpVersion, statusCode, status);
    if (body) {
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

/*
 * Read bytes string from an nsIInputStream.  If 'count' is omitted,
 * all available input is read.
 */
function readBytesFromInputStream(inputStream, count) {
  return CommonUtils.readBytesFromInputStream(inputStream, count);
}

/*
 * Ensure exceptions from inside callbacks leads to test failures.
 */
function ensureThrows(func) {
  return function() {
    try {
      func.apply(this, arguments);
    } catch (ex) {
      do_throw(ex);
    }
  };
}

/**
 * Proxy auth helpers.
 */

/**
 * Fake a PAC to prompt a channel replacement.
 */
var PACSystemSettings = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemProxySettings]),

  // Replace this URI for each test to avoid caching. We want to ensure that
  // each test gets a completely fresh setup.
  mainThreadOnly: true,
  PACURI: null,
  getProxyForURI: function getProxyForURI(aURI) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

var fakePACCID;
function installFakePAC() {
  _("Installing fake PAC.");
  fakePACCID = MockRegistrar.register("@mozilla.org/system-proxy-settings;1",
                                      PACSystemSettings);
}

function uninstallFakePAC() {
  _("Uninstalling fake PAC.");
  MockRegistrar.unregister(fakePACCID);
}


function getUptakeTelemetrySnapshot(key) {
  Cu.import("resource://gre/modules/Services.jsm");
  const TELEMETRY_HISTOGRAM_ID = "UPTAKE_REMOTE_CONTENT_RESULT_1";
  return Services.telemetry
           .getKeyedHistogramById(TELEMETRY_HISTOGRAM_ID)
           .snapshot(key);
}

function checkUptakeTelemetry(snapshot1, snapshot2, expectedIncrements) {
  const LABELS = ["up_to_date", "success", "backoff", "pref_disabled", "parse_error", "content_error", "sign_error", "sign_retry_error", "conflict_error", "sync_error", "apply_error", "server_error", "certificate_error", "download_error", "timeout_error", "network_error", "offline_error", "cleanup_error", "unknown_error", "custom_1_error", "custom_2_error", "custom_3_error", "custom_4_error", "custom_5_error"];
  for (const label of LABELS) {
    const key = LABELS.indexOf(label);
    const expected = expectedIncrements[label] || 0;
    const actual = snapshot2.counts[key] - snapshot1.counts[key];
    equal(expected, actual, `check histogram count for ${label}`);
  }
}
