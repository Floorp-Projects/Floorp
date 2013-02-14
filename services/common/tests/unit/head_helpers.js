/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/services-common/logging.js");

let btoa = Cu.import("resource://services-common/log4moz.js").btoa;
let atob = Cu.import("resource://services-common/log4moz.js").atob;

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
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));

/**
 * Obtain a port number to run a server on.
 *
 * In the ideal world, this would be dynamic so multiple servers could be run
 * in parallel.
 */
function get_server_port() {
  return 8080;
}

function httpd_setup (handlers, port) {
  let port   = port || 8080;
  let server = new HttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  try {
    server.start(port);
  } catch (ex) {
    _("==========================================");
    _("Got exception starting HTTP server on port " + port);
    _("Error: " + CommonUtils.exceptionStr(ex));
    _("Is there a process already listening on port " + port + "?");
    _("==========================================");
    do_throw(ex);
  }

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
let PACSystemSettings = {
  CID: Components.ID("{5645d2c1-d6d8-4091-b117-fe7ee4027db7}"),
  contractID: "@mozilla.org/system-proxy-settings;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory,
                                         Ci.nsISystemProxySettings]),

  createInstance: function createInstance(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },

  lockFactory: function lockFactory(lock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  // Replace this URI for each test to avoid caching. We want to ensure that
  // each test gets a completely fresh setup.
  mainThreadOnly: true,
  PACURI: null,
  getProxyForURI: function getProxyForURI(aURI) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }
};

function installFakePAC() {
  _("Installing fake PAC.");
  Cm.nsIComponentRegistrar
    .registerFactory(PACSystemSettings.CID,
                     "Fake system proxy-settings",
                     PACSystemSettings.contractID,
                     PACSystemSettings);
}

function uninstallFakePAC() {
  _("Uninstalling fake PAC.");
  let CID = PACSystemSettings.CID;
  Cm.nsIComponentRegistrar.unregisterFactory(CID, PACSystemSettings);
}
