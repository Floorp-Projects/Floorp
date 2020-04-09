/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head_global.js */

var { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
var { CommonUtils } = ChromeUtils.import("resource://services-common/utils.js");
var {
  HTTP_400,
  HTTP_401,
  HTTP_402,
  HTTP_403,
  HTTP_404,
  HTTP_405,
  HTTP_406,
  HTTP_407,
  HTTP_408,
  HTTP_409,
  HTTP_410,
  HTTP_411,
  HTTP_412,
  HTTP_413,
  HTTP_414,
  HTTP_415,
  HTTP_417,
  HTTP_500,
  HTTP_501,
  HTTP_502,
  HTTP_503,
  HTTP_504,
  HTTP_505,
  HttpError,
  HttpServer,
} = ChromeUtils.import("resource://testing-common/httpd.js");
var { getTestLogger, initTestLogging } = ChromeUtils.import(
  "resource://testing-common/services/common/logging.js"
);
var { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

function do_check_empty(obj) {
  do_check_attribute_count(obj, 0);
}

function do_check_attribute_count(obj, c) {
  Assert.equal(c, Object.keys(obj).length);
}

function do_check_throws(aFunc, aResult) {
  try {
    aFunc();
  } catch (e) {
    Assert.equal(e.result, aResult);
    return;
  }
  do_throw("Expected result " + aResult + ", none thrown.");
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
    Assert.equal(e.message, aResult);
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
  print(Array.from(arguments).join(" "));
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
  server.baseURI =
    i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;

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

function promiseStopServer(server) {
  return new Promise(resolve => server.stop(resolve));
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsISystemProxySettings]),

  // Replace this URI for each test to avoid caching. We want to ensure that
  // each test gets a completely fresh setup.
  mainThreadOnly: true,
  PACURI: null,
  getProxyForURI: function getProxyForURI(aURI) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

var fakePACCID;
function installFakePAC() {
  _("Installing fake PAC.");
  fakePACCID = MockRegistrar.register(
    "@mozilla.org/system-proxy-settings;1",
    PACSystemSettings
  );
}

function uninstallFakePAC() {
  _("Uninstalling fake PAC.");
  MockRegistrar.unregister(fakePACCID);
}

function _eventsTelemetrySnapshot(component, source) {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const TELEMETRY_CATEGORY_ID = "uptake.remotecontent.result";
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_ALL_CHANNELS,
    true
  );
  const parentEvents = snapshot.parent || [];
  return (
    parentEvents
      // Transform raw event data to objects.
      .map(([i, category, method, object, value, extras]) => {
        return { category, method, object, value, extras };
      })
      // Keep only for the specified component and source.
      .filter(
        e =>
          e.category == TELEMETRY_CATEGORY_ID &&
          e.object == component &&
          e.extras.source == source
      )
      // Return total number of events received by status, to mimic histograms snapshots.
      .reduce((acc, e) => {
        acc[e.value] = (acc[e.value] || 0) + 1;
        return acc;
      }, {})
  );
}

function getUptakeTelemetrySnapshot(key) {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const TELEMETRY_HISTOGRAM_ID = "UPTAKE_REMOTE_CONTENT_RESULT_1";
  const TELEMETRY_COMPONENT = "remotesettings";
  const histogram = Services.telemetry
    .getKeyedHistogramById(TELEMETRY_HISTOGRAM_ID)
    .snapshot()[key];
  const events = _eventsTelemetrySnapshot(TELEMETRY_COMPONENT, key);
  return { histogram, events };
}

function checkUptakeTelemetry(snapshot1, snapshot2, expectedIncrements) {
  const { UptakeTelemetry } = ChromeUtils.import(
    "resource://services-common/uptake-telemetry.js"
  );
  const STATUSES = Object.values(UptakeTelemetry.HISTOGRAM_LABELS);

  for (const status of STATUSES) {
    const key = STATUSES.indexOf(status);
    const expected = expectedIncrements[status] || 0;
    // Check histogram increments.
    let value1 =
      (snapshot1 && snapshot1.histogram && snapshot1.histogram.values[key]) ||
      0;
    let value2 =
      (snapshot2 && snapshot2.histogram && snapshot2.histogram.values[key]) ||
      0;
    let actual = value2 - value1;
    equal(expected, actual, `check histogram values for ${status}`);
    // Check events increments.
    value1 =
      (snapshot1 && snapshot1.histogram && snapshot1.histogram.values[key]) ||
      0;
    value2 =
      (snapshot2 && snapshot2.histogram && snapshot2.histogram.values[key]) ||
      0;
    actual = value2 - value1;
    equal(expected, actual, `check events for ${status}`);
  }
}

async function withFakeChannel(channel, f) {
  const module = ChromeUtils.import(
    "resource://services-common/uptake-telemetry.js",
    null
  );
  const oldPolicy = module.Policy;
  module.Policy = {
    ...oldPolicy,
    getChannel: () => channel,
  };
  try {
    return await f();
  } finally {
    module.Policy = oldPolicy;
  }
}

function arrayEqual(a, b) {
  return JSON.stringify(a) == JSON.stringify(b);
}
