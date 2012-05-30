/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ['loadFile','Collector','Runner','events',
                        'jsbridge', 'runTestFile', 'log', 'getThread',
                        'timers', 'persisted'];

var httpd = {};   Components.utils.import('resource://mozmill/stdlib/httpd.js', httpd);
var os = {};      Components.utils.import('resource://mozmill/stdlib/os.js', os);
var strings = {}; Components.utils.import('resource://mozmill/stdlib/strings.js', strings);
var arrays = {};  Components.utils.import('resource://mozmill/stdlib/arrays.js', arrays);
var withs = {};   Components.utils.import('resource://mozmill/stdlib/withs.js', withs);
var utils = {};   Components.utils.import('resource://mozmill/modules/utils.js', utils);
var securableModule = {};  Components.utils.import('resource://mozmill/stdlib/securable-module.js', securableModule);

var aConsoleService = Components.classes["@mozilla.org/consoleservice;1"].
     getService(Components.interfaces.nsIConsoleService);
var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
var subscriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                    .getService(Components.interfaces.mozIJSSubScriptLoader);
var uuidgen = Components.classes["@mozilla.org/uuid-generator;1"]
                    .getService(Components.interfaces.nsIUUIDGenerator);

var persisted = {};

var moduleLoader = new securableModule.Loader({
  rootPaths: ["resource://mozmill/modules/"],
  defaultPrincipal: "system",
  globals : { Cc: Components.classes,
              Ci: Components.interfaces,
              Cu: Components.utils,
              Cr: Components.results}
});

arrayRemove = function(array, from, to) {
  var rest = array.slice((to || from) + 1 || array.length);
  array.length = from < 0 ? array.length + from : from;
  return array.push.apply(array, rest);
};

mozmill = undefined; mozelement = undefined;

var loadTestResources = function () {
  // load resources we want in our tests
  if (mozmill == undefined) {
    mozmill = {};
    Components.utils.import("resource://mozmill/modules/mozmill.js", mozmill);
  }
  if (mozelement == undefined) {
    mozelement = {};
    Components.utils.import("resource://mozmill/modules/mozelement.js", mozelement);
  }
}

var loadFile = function(path, collector) {
  // load a test module from a file and add some candy
  var file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(path);
  var uri = ios.newFileURI(file).spec;

  loadTestResources();
  var assertions = moduleLoader.require("./assertions");
  var module = {
    collector:  collector,
    mozmill: mozmill,
    elementslib: mozelement,
    findElement: mozelement,
    persisted: persisted,
    Cc: Components.classes,
    Ci: Components.interfaces,
    Cu: Components.utils,
    Cr: Components.results,
    log: log,
    assert: new assertions.Assert(),
    expect: new assertions.Expect()
  }

  module.require = function (mod) {
    var loader = new securableModule.Loader({
      rootPaths: [ios.newFileURI(file.parent).spec,
                  "resource://mozmill/modules/"],
      defaultPrincipal: "system",
      globals : { mozmill: mozmill,
                  elementslib: mozelement,      // This a quick hack to maintain backwards compatibility with 1.5.x
                  findElement: mozelement,
                  persisted: persisted,
                  Cc: Components.classes,
                  Ci: Components.interfaces,
                  Cu: Components.utils,
                  log: log }
    });
    return loader.require(mod);
  }

  if (collector != undefined) {
    collector.current_file = file;
    collector.current_path = path;
  }
  try {
    subscriptLoader.loadSubScript(uri, module, "UTF-8");
  } catch(e) {
    events.fail(e);
    var obj = {
      'filename':path,
      'passed':false,
      'failed':true,
      'passes':0,
      'fails' :1,
      'name'  :'Unknown Test',
    };
    events.fireEvent('endTest', obj);
    Components.utils.reportError(e);
  }

  module.__file__ = path;
  module.__uri__ = uri;
  return module;
}

function stateChangeBase (possibilties, restrictions, target, cmeta, v) {
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

timers = [];

var events = {
  'currentState' : null,
  'currentModule': null,
  'currentTest'  : null,
  'userShutdown' : false,
  'appQuit'      : false,
  'listeners'    : {},
}
events.setState = function (v) {
   return stateChangeBase(['dependencies', 'setupModule', 'teardownModule',
                           'setupTest', 'teardownTest', 'test', 'collection'],
                           null, 'currentState', 'setState', v);
}
events.toggleUserShutdown = function (obj){
  if (this.userShutdown) {
      this.fail({'function':'frame.events.toggleUserShutdown', 'message':'Shutdown expected but none detected before timeout', 'userShutdown': obj});
  }
  this.userShutdown = obj;
}
events.isUserShutdown = function () {
  return Boolean(this.userShutdown);
}
events.setTest = function (test, invokedFromIDE) {
  test.__passes__ = [];
  test.__fails__ = [];
  test.__invokedFromIDE__ = invokedFromIDE;
  events.currentTest = test;
  test.__start__ = Date.now();
  var obj = {'filename':events.currentModule.__file__,
             'name':test.__name__,
            }
  events.fireEvent('setTest', obj);
}
events.endTest = function (test) {
  // report the end of a test
  test.status = 'done';
  events.currentTest = null;
  test.__end__ = Date.now();
  var obj = {'filename':events.currentModule.__file__,
         'passed':test.__passes__.length,
         'failed':test.__fails__.length,
         'passes':test.__passes__,
         'fails' :test.__fails__,
         'name'  :test.__name__,
         'time_start':test.__start__,
         'time_end':test.__end__
         }
  if (test.skipped) {
    obj['skipped'] = true;
    obj.skipped_reason = test.skipped_reason;
  }
  if (test.meta) {
    obj.meta = test.meta;
  }

  // Report the test result only if the test is a true test or if it is a
  // failing setup/teardown
  var shouldSkipReporting = false;
  if (test.__passes__ &&
      (test.__name__ == 'setupModule' ||
       test.__name__ == 'setupTest' ||
       test.__name__ == 'teardownTest' ||
       test.__name__ == 'teardownModule')) {
    shouldSkipReporting = true;
  }

  if (!shouldSkipReporting) {
    events.fireEvent('endTest', obj);
  }
}

events.setModule = function (v) {
  return stateChangeBase( null, [function (v) {return (v.__file__ != undefined)}],
                          'currentModule', 'setModule', v);
}

events.pass = function (obj) {
  // a low level event, such as a keystroke, succeeds
  if (events.currentTest) {
    events.currentTest.__passes__.push(obj);
  }
  for each(var timer in timers) {
    timer.actions.push(
      {"currentTest":events.currentModule.__file__+"::"+events.currentTest.__name__, "obj":obj,
       "result":"pass"}
    );
  }
  events.fireEvent('pass', obj);
}
events.fail = function (obj) {
  var error = obj.exception;
  if(error) {
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
  for each(var time in timers) {
    timer.actions.push(
      {"currentTest":events.currentModule.__file__+"::"+events.currentTest.__name__, "obj":obj,
       "result":"fail"}
    );
  }
  events.fireEvent('fail', obj);
}
events.skip = function (reason) {
  // this is used to report skips associated with setupModule and setupTest
  // and nothing else
  events.currentTest.skipped = true;
  events.currentTest.skipped_reason = reason;
  for each(var timer in timers) {
    timer.actions.push(
      {"currentTest":events.currentModule.__file__+"::"+events.currentTest.__name__, "obj":reason,
       "result":"skip"}
    );
  }
  events.fireEvent('skip', reason);
}
events.fireEvent = function (name, obj) {
  if (this.listeners[name]) {
    for (var i in this.listeners[name]) {
      this.listeners[name][i](obj);
    }
  }
  for each(var listener in this.globalListeners) {
    listener(name, obj);
  }
}
events.globalListeners = [];
events.addListener = function (name, listener) {
  if (this.listeners[name]) {
    this.listeners[name].push(listener);
  } else if (name =='') {
    this.globalListeners.push(listener)
  } else {
    this.listeners[name] = [listener];
  }
}
events.removeListener = function(listener) {
  for (var listenerIndex in this.listeners) {
    var e = this.listeners[listenerIndex];
    for (var i in e){
      if (e[i] == listener) {
        this.listeners[listenerIndex] = arrayRemove(e, i);
      }
    }
  }
  for (var i in this.globalListeners) {
    if (this.globalListeners[i] == listener) {
      this.globalListeners = arrayRemove(this.globalListeners, i);
    }
  }
}

var log = function (obj) {
  events.fireEvent('log', obj);
}

try {
  var jsbridge = {}; Components.utils.import('resource://jsbridge/modules/events.js', jsbridge);
} catch(err) {
  var jsbridge = null;

  aConsoleService.logStringMessage("jsbridge not available.");
}

if (jsbridge) {
  events.addListener('', function (name, obj) {jsbridge.fireEvent('mozmill.'+name, obj)} );
}

function Collector () {
  // the collector handles HTTPD and initilizing the module
  this.test_modules_by_filename = {};
  this.testing = [];
  this.httpd_started = false;
  this.http_port = 43336;
  this.http_server = httpd.getServer(this.http_port);
}

Collector.prototype.startHttpd = function () {
  while (this.httpd == undefined) {
    try {
      this.http_server.start(this.http_port);
      this.httpd = this.http_server;
    } catch(e) { // Failure most likely due to port conflict
      this.http_port++;
      this.http_server = httpd.getServer(this.http_port);
    };
  }
}
Collector.prototype.stopHttpd = function () {
  if (this.httpd) {
    this.httpd.stop(function(){});  // Callback needed to pause execution until the server has been properly shutdown
    this.httpd = null;
  }
}
Collector.prototype.addHttpResource = function (directory, ns) {
  if (!this.httpd) {
    this.startHttpd();
  }

  if (!ns) {
    ns = '/';
  } else {
    ns = '/' + ns + '/';
  }

  var lp = Components.classes["@mozilla.org/file/local;1"].
           createInstance(Components.interfaces.nsILocalFile);
  lp.initWithPath(os.abspath(directory, this.current_file));
  this.httpd.registerDirectory(ns, lp);

  return 'http://localhost:' + this.http_port + ns
}

Collector.prototype.initTestModule = function (filename, name) {
  var test_module = loadFile(filename, this);
  test_module.__tests__ = [];
  for (var i in test_module) {
    if (typeof(test_module[i]) == "function") {
      test_module[i].__name__ = i;
      if (i == "setupTest") {
        test_module.__setupTest__ = test_module[i];
      } else if (i == "setupModule") {
        test_module.__setupModule__ = test_module[i];
      } else if (i == "teardownTest") {
        test_module.__teardownTest__ = test_module[i];
      } else if (i == "teardownModule") {
        test_module.__teardownModule__ = test_module[i];
      } else if (withs.startsWith(i, "test")) {
        if (name && (i != name)) {
            continue;
        }
        name = null;
        test_module.__tests__.push(test_module[i]);
      }
    }
  }

  test_module.collector = this;
  test_module.status = 'loaded';
  this.test_modules_by_filename[filename] = test_module;
  return test_module;
}

// Observer which gets notified when the application quits
function AppQuitObserver() {
  this.register();
}
AppQuitObserver.prototype = {
  observe: function(subject, topic, data) {
    events.appQuit = true;
  },
  register: function() {
    var obsService = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
    obsService.addObserver(this, "quit-application", false);
  },
  unregister: function() {
    var obsService = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Components.interfaces.nsIObserverService);
    obsService.removeObserver(this, "quit-application");
  }
}


function Runner (collector, invokedFromIDE) {
  this.collector = collector;
  this.invokedFromIDE = invokedFromIDE
  events.fireEvent('startRunner', true);
  var m = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', m);
  this.platform = m.platform;
}

Runner.prototype.runTestFile = function (filename, name) {
  this.collector.initTestModule(filename, name);
  this.runTestModule(this.collector.test_modules_by_filename[filename]);
}
Runner.prototype.end = function () {
  try {
    events.fireEvent('persist', persisted);
  } catch(e) {
    events.fireEvent('error', "persist serialization failed.");
  }
  this.collector.stopHttpd();
  events.fireEvent('endRunner', true);
}

Runner.prototype.wrapper = function (func, arg) {
  thread = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager)
                     .currentThread;

  // skip excluded platforms
  if (func.EXCLUDED_PLATFORMS != undefined) {
    if (arrays.inArray(func.EXCLUDED_PLATFORMS, this.platform)) {
      events.skip("Platform exclusion");
      return;
    }
  }

  // skip function if requested
  if (func.__force_skip__ != undefined) {
    events.skip(func.__force_skip__);
    return;
  }

  // execute the test function
  try {
    if (arg) {
        func(arg);
    } else {
        func();
    }

    // If a user shutdown was expected but the application hasn't quit, throw a failure
    if (events.isUserShutdown()) {
      utils.sleep(500);  // Prevents race condition between mozrunner hard process kill and normal FFx shutdown
      if (events.userShutdown['user'] && !events.appQuit) {
          events.fail({'function':'Runner.wrapper',
                       'message':'Shutdown expected but none detected before end of test',
                       'userShutdown': events.userShutdown});
      }
    }
  } catch (e) {
    // Allow the exception if a user shutdown was expected
    if (!events.isUserShutdown()) {
      events.fail({'exception': e, 'test':func})
      Components.utils.reportError(e);
    }
  }
}

Runner.prototype.runTestModule = function (module) {
  events.setModule(module);
  module.__status__ = 'running';
  if (module.__setupModule__) {
    events.setState('setupModule');
    events.setTest(module.__setupModule__);
    this.wrapper(module.__setupModule__, module);
    var setupModulePassed = (events.currentTest.__fails__.length == 0 && !events.currentTest.skipped);
    events.endTest(module.__setupModule__);
  } else {
    var setupModulePassed = true;
  }
  if (setupModulePassed) {
    var observer = new AppQuitObserver();
    for (var i in module.__tests__) {
      events.appQuit = false;
      var test = module.__tests__[i];

      // TODO: introduce per-test timeout:
      // https://bugzilla.mozilla.org/show_bug.cgi?id=574871

      if (module.__setupTest__) {
        events.setState('setupTest');
        events.setTest(module.__setupTest__);
        this.wrapper(module.__setupTest__, test);
        var setupTestPassed = (events.currentTest.__fails__.length == 0 && !events.currentTest.skipped);
        events.endTest(module.__setupTest__);
      } else {
        var setupTestPassed = true;
      }
      events.setState('test');
      events.setTest(test, this.invokedFromIDE);
      if (setupTestPassed) {
        this.wrapper(test);
        if (events.userShutdown && !events.userShutdown['user']) {
            events.endTest(test);
            break;
        }
      } else {
        events.skip("setupTest failed.");
      }
      if (module.__teardownTest__) {
        events.setState('teardownTest');
        events.setTest(module.__teardownTest__);
        this.wrapper(module.__teardownTest__, test);
        events.endTest(module.__teardownTest__);
      }
      events.endTest(test)
    }
    observer.unregister();
  } else {
    for each(var test in module.__tests__) {
      events.setTest(test);
      events.skip("setupModule failed.");
      events.endTest(test);
    }
  }
  if (module.__teardownModule__) {
    events.setState('teardownModule');
    events.setTest(module.__teardownModule__);
    this.wrapper(module.__teardownModule__, module);
    events.endTest(module.__teardownModule__);
  }
  module.__status__ = 'done';
}

var runTestFile = function (filename, invokedFromIDE, name) {
  var runner = new Runner(new Collector(), invokedFromIDE);
  runner.runTestFile(filename, name);
  runner.end();
  return true;
}

var getThread = function () {
  return thread;
}
