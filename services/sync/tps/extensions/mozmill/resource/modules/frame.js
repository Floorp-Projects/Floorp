/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['Collector','Runner','events', 'runTestFile', 'log',
                        'timers', 'persisted', 'shutdownApplication'];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

const TIMEOUT_SHUTDOWN_HTTPD = 15000;

Cu.import("resource://gre/modules/Services.jsm");

Cu.import('resource://mozmill/stdlib/httpd.js');

var broker = {};  Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var assertions = {}; Cu.import('resource://mozmill/modules/assertions.js', assertions);
var errors = {}; Cu.import('resource://mozmill/modules/errors.js', errors);
var os = {};      Cu.import('resource://mozmill/stdlib/os.js', os);
var strings = {}; Cu.import('resource://mozmill/stdlib/strings.js', strings);
var arrays = {};  Cu.import('resource://mozmill/stdlib/arrays.js', arrays);
var withs = {};   Cu.import('resource://mozmill/stdlib/withs.js', withs);
var utils = {};   Cu.import('resource://mozmill/stdlib/utils.js', utils);

var securableModule = {};
Cu.import('resource://mozmill/stdlib/securable-module.js', securableModule);

var uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

var httpd = null;
var persisted = {};

var assert = new assertions.Assert();
var expect = new assertions.Expect();

var mozmill = undefined;
var mozelement = undefined;
var modules = undefined;

var timers = [];


/**
 * Shutdown or restart the application
 *
 * @param {boolean} [aFlags=undefined]
 *        Additional flags how to handle the shutdown or restart. The attributes
 *        eRestarti386 and eRestartx86_64 have not been documented yet.
 * @see https://developer.mozilla.org/nsIAppStartup#Attributes
 */
function shutdownApplication(aFlags) {
  var flags = Ci.nsIAppStartup.eForceQuit;

  if (aFlags) {
    flags |= aFlags;
  }

  // Send a request to shutdown the application. That will allow us and other
  // components to finish up with any shutdown code. Please note that we don't
  // care if other components or add-ons want to prevent this via cancelQuit,
  // we really force the shutdown.
  let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"].
                   createInstance(Components.interfaces.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);

  // Use a timer to trigger the application restart, which will allow us to
  // send an ACK packet via jsbridge if the method has been called via Python.
  var event = {
    notify: function(timer) {
      Services.startup.quit(flags);
    }
  }

  var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(event, 100, Ci.nsITimer.TYPE_ONE_SHOT);
}

function stateChangeBase(possibilties, restrictions, target, cmeta, v) {
  if (possibilties) {
    if (!arrays.inArray(possibilties, v)) {
      // TODO Error value not in this.poss
      return;
    }
  }

  if (restrictions) {
    for (var i in restrictions) {
      var r = restrictions[i];
      if (!r(v)) {
        // TODO error value did not pass restriction
        return;
      }
    }
  }

  // Fire jsbridge notification, logging notification, listener notifications
  events[target] = v;
  events.fireEvent(cmeta, target);
}


var events = {
  appQuit           : false,
  currentModule     : null,
  currentState      : null,
  currentTest       : null,
  shutdownRequested : false,
  userShutdown      : null,
  userShutdownTimer : null,

  listeners       : {},
  globalListeners : []
}

events.setState = function (v) {
  return stateChangeBase(['dependencies', 'setupModule', 'teardownModule',
                          'test', 'setupTest', 'teardownTest', 'collection'],
                          null, 'currentState', 'setState', v);
}

events.toggleUserShutdown = function (obj){
  if (!this.userShutdown) {
    this.userShutdown = obj;

    var event = {
      notify: function(timer) {
       events.toggleUserShutdown(obj);
      }
    }

    this.userShutdownTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.userShutdownTimer.initWithCallback(event, obj.timeout, Ci.nsITimer.TYPE_ONE_SHOT);

  } else {
    this.userShutdownTimer.cancel();

    // If the application is not going to shutdown, the user shutdown failed and
    // we have to force a shutdown.
    if (!events.appQuit) {
      this.fail({'function':'events.toggleUserShutdown',
                 'message':'Shutdown expected but none detected before timeout',
                 'userShutdown': obj});

      var flags = Ci.nsIAppStartup.eAttemptQuit;
      if (events.isRestartShutdown()) {
        flags |= Ci.nsIAppStartup.eRestart;
      }

      shutdownApplication(flags);
    }
  }
}

events.isUserShutdown = function () {
  return this.userShutdown ? this.userShutdown["user"] : false;
}

events.isRestartShutdown = function () {
  return this.userShutdown.restart;
}

events.startShutdown = function (obj) {
  events.fireEvent('shutdown', obj);

  if (obj["user"]) {
    events.toggleUserShutdown(obj);
  } else {
    shutdownApplication(obj.flags);
  }
}

events.setTest = function (test) {
  test.__start__ = Date.now();
  test.__passes__ = [];
  test.__fails__ = [];

  events.currentTest = test;

  var obj = {'filename': events.currentModule.__file__,
             'name': test.__name__}
  events.fireEvent('setTest', obj);
}

events.endTest = function (test) {
  // use the current test unless specified
  if (test === undefined) {
    test = events.currentTest;
  }

  // If no test is set it has already been reported. Beside that we don't want
  // to report it a second time.
  if (!test || test.status === 'done')
    return;

  // report the end of a test
  test.__end__ = Date.now();
  test.status = 'done';

  var obj = {'filename': events.currentModule.__file__,
             'passed': test.__passes__.length,
             'failed': test.__fails__.length,
             'passes': test.__passes__,
             'fails' : test.__fails__,
             'name'  : test.__name__,
             'time_start': test.__start__,
             'time_end': test.__end__}

  if (test.skipped) {
    obj['skipped'] = true;
    obj.skipped_reason = test.skipped_reason;
  }

  if (test.meta) {
    obj.meta = test.meta;
  }

  // Report the test result only if the test is a true test or if it is failing
  if (withs.startsWith(test.__name__, "test") || test.__fails__.length > 0) {
    events.fireEvent('endTest', obj);
  }
}

events.setModule = function (aModule) {
  aModule.__start__ = Date.now();
  aModule.__status__ = 'running';

  var result = stateChangeBase(null,
                               [function (aModule) {return (aModule.__file__ != undefined)}],
                               'currentModule', 'setModule', aModule);

  return result;
}

events.endModule = function (aModule) {
  // It should only reported once, so check if it already has been done
  if (aModule.__status__ === 'done')
    return;

  aModule.__end__ = Date.now();
  aModule.__status__ = 'done';

  var obj = {
    'filename': aModule.__file__,
    'time_start': aModule.__start__,
    'time_end': aModule.__end__
  }

  events.fireEvent('endModule', obj);
}

events.pass = function (obj) {
  // a low level event, such as a keystroke, succeeds
  if (events.currentTest) {
    events.currentTest.__passes__.push(obj);
  }

  for (var timer of timers) {
    timer.actions.push(
      {"currentTest": events.currentModule.__file__ + "::" + events.currentTest.__name__,
       "obj": obj,
       "result": "pass"}
    );
  }

  events.fireEvent('pass', obj);
}

events.fail = function (obj) {
  var error = obj.exception;

  if (error) {
    // Error objects aren't enumerable https://bugzilla.mozilla.org/show_bug.cgi?id=637207
    obj.exception = {
      name: error.name,
      message: error.message,
      lineNumber: error.lineNumber,
      fileName: error.fileName,
      stack: error.stack
    };
  }

  // a low level event, such as a keystroke, fails
  if (events.currentTest) {
    events.currentTest.__fails__.push(obj);
  }

  for (var time of timers) {
    timer.actions.push(
      {"currentTest": events.currentModule.__file__ + "::" + events.currentTest.__name__,
       "obj": obj,
       "result": "fail"}
    );
  }

  events.fireEvent('fail', obj);
}

events.skip = function (reason) {
  // this is used to report skips associated with setupModule and nothing else
  events.currentTest.skipped = true;
  events.currentTest.skipped_reason = reason;

  for (var timer of timers) {
    timer.actions.push(
      {"currentTest": events.currentModule.__file__ + "::" + events.currentTest.__name__,
       "obj": reason,
       "result": "skip"}
    );
  }

  events.fireEvent('skip', reason);
}

events.fireEvent = function (name, obj) {
  if (events.appQuit) {
    // dump('* Event discarded: ' + name + ' ' + JSON.stringify(obj) + '\n');
    return;
  }

  if (this.listeners[name]) {
    for (var i in this.listeners[name]) {
      this.listeners[name][i](obj);
    }
  }

  for (var listener of this.globalListeners) {
    listener(name, obj);
  }
}

events.addListener = function (name, listener) {
  if (this.listeners[name]) {
    this.listeners[name].push(listener);
  } else if (name == '') {
    this.globalListeners.push(listener)
  } else {
    this.listeners[name] = [listener];
  }
}

events.removeListener = function (listener) {
  for (var listenerIndex in this.listeners) {
    var e = this.listeners[listenerIndex];

    for (var i in e){
      if (e[i] == listener) {
        this.listeners[listenerIndex] = arrays.remove(e, i);
      }
    }
  }

  for (var i in this.globalListeners) {
    if (this.globalListeners[i] == listener) {
      this.globalListeners = arrays.remove(this.globalListeners, i);
    }
  }
}

events.persist = function () {
  try {
    events.fireEvent('persist', persisted);
  } catch (e) {
    events.fireEvent('error', "persist serialization failed.")
  }
}

events.firePythonCallback = function (obj) {
  obj['test'] = events.currentModule.__file__;
  events.fireEvent('firePythonCallback', obj);
}

events.screenshot = function (obj) {
  // Find the name of the test function
  for (var attr in events.currentModule) {
    if (events.currentModule[attr] == events.currentTest) {
      var testName = attr;
      break;
    }
  }

  obj['test_file'] = events.currentModule.__file__;
  obj['test_name'] = testName;
  events.fireEvent('screenshot', obj);
}

var log = function (obj) {
  events.fireEvent('log', obj);
}

// Register the listeners
broker.addObject({'endTest': events.endTest,
                  'fail': events.fail,
                  'firePythonCallback': events.firePythonCallback,
                  'log': log,
                  'pass': events.pass,
                  'persist': events.persist,
                  'screenshot': events.screenshot,
                  'shutdown': events.startShutdown,
                 });

try {
  Cu.import('resource://jsbridge/modules/Events.jsm');

  events.addListener('', function (name, obj) {
    Events.fireEvent('mozmill.' + name, obj);
  });
} catch (e) {
  Services.console.logStringMessage("Event module of JSBridge not available.");
}


/**
 * Observer for notifications when the application is going to shutdown
 */
function AppQuitObserver() {
  this.runner = null;

  Services.obs.addObserver(this, "quit-application-requested");
}

AppQuitObserver.prototype = {
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-requested":
        Services.obs.removeObserver(this, "quit-application-requested");

        // If we observe a quit notification make sure to send the
        // results of the current test. In those cases we don't reach
        // the equivalent code in runTestModule()
        events.pass({'message': 'AppQuitObserver: ' + JSON.stringify(aData),
                     'userShutdown': events.userShutdown});

        if (this.runner) {
          this.runner.end();
        }

        if (httpd) {
          httpd.stop();
        }

        events.appQuit = true;

        break;
    }
  }
}

var appQuitObserver = new AppQuitObserver();

/**
 * The collector handles HTTPd.js and initilizing the module
 */
function Collector() {
  this.test_modules_by_filename = {};
  this.testing = [];
}

Collector.prototype.addHttpResource = function (aDirectory, aPath) {
  var fp = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  fp.initWithPath(os.abspath(aDirectory, this.current_file));

  return httpd.addHttpResource(fp, aPath);
}

Collector.prototype.initTestModule = function (filename, testname) {
  var test_module = this.loadFile(filename, this);
  var has_restarted = !(testname == null);
  test_module.__tests__ = [];

  for (var i in test_module) {
    if (typeof(test_module[i]) == "function") {
      test_module[i].__name__ = i;

      // Only run setupModule if we are a single test OR if we are the first
      // test of a restart chain (don't run it prior to members in a restart
      // chain)
      if (i == "setupModule" && !has_restarted) {
        test_module.__setupModule__ = test_module[i];
      } else if (i == "setupTest") {
        test_module.__setupTest__ = test_module[i];
      } else if (i == "teardownTest") {
        test_module.__teardownTest__ = test_module[i];
      } else if (i == "teardownModule") {
        test_module.__teardownModule__ = test_module[i];
      } else if (withs.startsWith(i, "test")) {
        if (testname && (i != testname)) {
          continue;
        }

        testname = null;
        test_module.__tests__.push(test_module[i]);
      }
    }
  }

  test_module.collector = this;
  test_module.status = 'loaded';

  this.test_modules_by_filename[filename] = test_module;

  return test_module;
}

Collector.prototype.loadFile = function (path, collector) {
  var moduleLoader = new securableModule.Loader({
    rootPaths: ["resource://mozmill/modules/"],
    defaultPrincipal: "system",
    globals : { Cc: Cc,
                Ci: Ci,
                Cu: Cu,
                Cr: Components.results}
  });

  // load a test module from a file and add some candy
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  var uri = Services.io.newFileURI(file).spec;

  this.loadTestResources();

  var systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  var module = new Components.utils.Sandbox(systemPrincipal);
  module.assert = assert;
  module.Cc = Cc;
  module.Ci = Ci;
  module.Cr = Components.results;
  module.Cu = Cu;
  module.collector = collector;
  module.driver = moduleLoader.require("driver");
  module.elementslib = mozelement;
  module.errors = errors;
  module.expect = expect;
  module.findElement = mozelement;
  module.log = log;
  module.mozmill = mozmill;
  module.persisted = persisted;

  module.require = function (mod) {
    var loader = new securableModule.Loader({
      rootPaths: [Services.io.newFileURI(file.parent).spec,
                  "resource://mozmill/modules/"],
      defaultPrincipal: "system",
      globals : { assert: assert,
                  expect: expect,
                  mozmill: mozmill,
                  elementslib: mozelement,      // This a quick hack to maintain backwards compatibility with 1.5.x
                  findElement: mozelement,
                  persisted: persisted,
                  Cc: Cc,
                  Ci: Ci,
                  Cu: Cu,
                  log: log }
    });

    if (modules != undefined) {
      loader.modules = modules;
    }

    var retval = loader.require(mod);
    modules = loader.modules;

    return retval;
  }

  if (collector != undefined) {
    collector.current_file = file;
    collector.current_path = path;
  }

  try {
    Services.scriptloader.loadSubScript(uri, module, "UTF-8");
  } catch (e) {
    var obj = {
      'filename': path,
      'passed': 0,
      'failed': 1,
      'passes': [],
      'fails' : [{'exception' : {
                    message: e.message,
                    filename: e.filename,
                    lineNumber: e.lineNumber}}],
      'name'  :'<TOP_LEVEL>'
    };

    events.fail({'exception': e});
    events.fireEvent('endTest', obj);
  }

  module.__file__ = path;
  module.__uri__ = uri;

  return module;
}

Collector.prototype.loadTestResources = function () {
  // load resources we want in our tests
  if (mozmill === undefined) {
    mozmill = {};
    Cu.import("resource://mozmill/driver/mozmill.js", mozmill);
  }
  if (mozelement === undefined) {
    mozelement = {};
    Cu.import("resource://mozmill/driver/mozelement.js", mozelement);
  }
}


/**
 *
 */
function Httpd(aPort) {
  this.http_port = aPort;

  while (true) {
    try {
      var srv = new HttpServer();
      srv.registerContentType("sjs", "sjs");
      srv.identity.setPrimary("http", "localhost", this.http_port);
      srv.start(this.http_port);

      this._httpd = srv;
      break;
    }
    catch (e) {
      // Failure most likely due to port conflict
      this.http_port++;
    }
  }
}

Httpd.prototype.addHttpResource = function (aDir, aPath) {
  var path = aPath ? ("/" + aPath + "/") : "/";

  try {
    this._httpd.registerDirectory(path, aDir);
    return 'http://localhost:' + this.http_port + path;
  }
  catch (e) {
    throw Error("Failure to register directory: " + aDir.path);
  }
};

Httpd.prototype.stop = function () {
  if (!this._httpd) {
    return;
  }

  var shutdown = false;
  this._httpd.stop(function () { shutdown = true; });

  assert.waitFor(function () {
    return shutdown;
  }, "Local HTTP server has been stopped", TIMEOUT_SHUTDOWN_HTTPD);

  this._httpd = null;
};

function startHTTPd() {
  if (!httpd) {
    // Ensure that we start the HTTP server only once during a session
    httpd = new Httpd(43336);
  }
}


function Runner() {
  this.collector = new Collector();
  this.ended = false;

  var m = {}; Cu.import('resource://mozmill/driver/mozmill.js', m);
  this.platform = m.platform;

  events.fireEvent('startRunner', true);
}

Runner.prototype.end = function () {
  if (!this.ended) {
    this.ended = true;

    appQuitObserver.runner = null;

    events.endTest();
    events.endModule(events.currentModule);
    events.fireEvent('endRunner', true);
    events.persist();
  }
};

Runner.prototype.runTestFile = function (filename, name) {
  var module = this.collector.initTestModule(filename, name);
  this.runTestModule(module);
};

Runner.prototype.runTestModule = function (module) {
  appQuitObserver.runner = this;
  events.setModule(module);

  // If setupModule passes, run all the tests. Otherwise mark them as skipped.
  if (this.execFunction(module.__setupModule__, module)) {
    for (var test of module.__tests__) {
      if (events.shutdownRequested) {
        break;
      }

      // If setupTest passes, run the test. Otherwise mark it as skipped.
      if (this.execFunction(module.__setupTest__, module)) {
        this.execFunction(test);
      } else {
        this.skipFunction(test, module.__setupTest__.__name__ + " failed");
      }

      this.execFunction(module.__teardownTest__, module);
    }

  } else {
    for (var test of module.__tests__) {
      this.skipFunction(test, module.__setupModule__.__name__ + " failed");
    }
  }

  this.execFunction(module.__teardownModule__, module);
  events.endModule(module);
};

Runner.prototype.execFunction = function (func, arg) {
  if (typeof func !== "function" || events.shutdownRequested) {
    return true;
  }

  var isTest = withs.startsWith(func.__name__, "test");

  events.setState(isTest ? "test" : func.__name);
  events.setTest(func);

  // skip excluded platforms
  if (func.EXCLUDED_PLATFORMS != undefined) {
    if (arrays.inArray(func.EXCLUDED_PLATFORMS, this.platform)) {
      events.skip("Platform exclusion");
      events.endTest(func);
      return false;
    }
  }

  // skip function if requested
  if (func.__force_skip__ != undefined) {
    events.skip(func.__force_skip__);
    events.endTest(func);
    return false;
  }

  // execute the test function
  try {
    func(arg);
  } catch (e) {
    if (e instanceof errors.ApplicationQuitError) {
      events.shutdownRequested = true;
    } else {
      events.fail({'exception': e, 'test': func})
    }
  }

  // If a user shutdown has been requested and the function already returned,
  // we can assume that a shutdown will not happen anymore. We should force a
  // shutdown then, to prevent the next test from being executed.
  if (events.isUserShutdown()) {
    events.shutdownRequested = true;
    events.toggleUserShutdown(events.userShutdown);
  }

  events.endTest(func);
  return events.currentTest.__fails__.length == 0;
};

function runTestFile(filename, name) {
  var runner = new Runner();
  runner.runTestFile(filename, name);
  runner.end();

  return true;
}

Runner.prototype.skipFunction = function (func, message) {
  events.setTest(func);
  events.skip(message);
  events.endTest(func);
};
