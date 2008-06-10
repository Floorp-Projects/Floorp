version(180);

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

let ds = Cc["@mozilla.org/file/directory_service;1"]
  .getService(Ci.nsIProperties);

let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == "ExtPrefDL") {
      return [ds.get("CurProcD", Ci.nsIFile)];
    }
    throw Cr.NS_ERROR_FAILURE;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
ds.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

do_bind_resource(do_get_file("modules"), "weave");

function loadInSandbox(aUri) {
  var sandbox = Components.utils.Sandbox(this);
  var request = Components.
                classes["@mozilla.org/xmlextras/xmlhttprequest;1"].
                createInstance();

  request.open("GET", aUri, false);
  request.send(null);
  Components.utils.evalInSandbox(request.responseText, sandbox);

  return sandbox;
}

function makeAsyncTestRunner(generator) {
  const Cu = Components.utils;

  Cu.import("resource://weave/log4moz.js");
  Cu.import("resource://weave/async.js");

  var errorsLogged = 0;

  function _TestFormatter() {}
  _TestFormatter.prototype = {
    format: function BF_format(message) {
      if (message.level == Log4Moz.Level.Error)
        errorsLogged += 1;
      return message.loggerName + "\t" + message.levelDesc + "\t" +
        message.message + "\n";
    }
  };
  _TestFormatter.prototype.__proto__ = new Log4Moz.Formatter();

  var log = Log4Moz.Service.rootLogger;
  var formatter = new _TestFormatter();
  var appender = new Log4Moz.DumpAppender(formatter);
  log.level = Log4Moz.Level.Debug;
  appender.level = Log4Moz.Level.Debug;
  log.addAppender(appender);

  function run_test() {
    do_test_pending();

    let onComplete = function() {
      if (errorsLogged)
        do_throw("Errors were logged.");
      else
        do_test_finished();
    };

    Async.run({}, generator, onComplete);
  }

  return run_test;
}
