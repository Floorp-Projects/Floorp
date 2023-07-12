/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head_global.js */

var { Log } = ChromeUtils.importESModule("resource://gre/modules/Log.sys.mjs");
var { CommonUtils } = ChromeUtils.importESModule(
  "resource://services-common/utils.sys.mjs"
);
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
} = ChromeUtils.importESModule("resource://testing-common/httpd.sys.mjs");
var { getTestLogger, initTestLogging } = ChromeUtils.importESModule(
  "resource://testing-common/services/common/logging.sys.mjs"
);
var { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);
var { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
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
var _ = function (some, debug, text, to) {
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
  if (!count) {
    count = inputStream.available();
  }
  if (!count) {
    return "";
  }
  return NetUtil.readInputStreamToString(inputStream, count, {
    charset: "UTF-8",
  });
}

function writeBytesToOutputStream(outputStream, string) {
  if (!string) {
    return;
  }
  let converter = Cc[
    "@mozilla.org/intl/converter-output-stream;1"
  ].createInstance(Ci.nsIConverterOutputStream);
  converter.init(outputStream, "UTF-8");
  converter.writeString(string);
  converter.close();
}

/*
 * Ensure exceptions from inside callbacks leads to test failures.
 */
function ensureThrows(func) {
  return function () {
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
  QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),

  // Replace this URI for each test to avoid caching. We want to ensure that
  // each test gets a completely fresh setup.
  mainThreadOnly: true,
  PACURI: null,
  getProxyForURI: function getProxyForURI(aURI) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
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

function getUptakeTelemetrySnapshot(component, source) {
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

function checkUptakeTelemetry(snapshot1, snapshot2, expectedIncrements) {
  const { UptakeTelemetry } = ChromeUtils.importESModule(
    "resource://services-common/uptake-telemetry.sys.mjs"
  );
  const STATUSES = Object.values(UptakeTelemetry.STATUS);
  for (const status of STATUSES) {
    const expected = expectedIncrements[status] || 0;
    const previous = snapshot1[status] || 0;
    const current = snapshot2[status] || previous;
    Assert.equal(expected, current - previous, `check events for ${status}`);
  }
}

async function withFakeChannel(channel, f) {
  const { Policy } = ChromeUtils.importESModule(
    "resource://services-common/uptake-telemetry.sys.mjs"
  );
  let oldGetChannel = Policy.getChannel;
  Policy.getChannel = () => channel;
  try {
    return await f();
  } finally {
    Policy.getChannel = oldGetChannel;
  }
}

function arrayEqual(a, b) {
  return JSON.stringify(a) == JSON.stringify(b);
}
