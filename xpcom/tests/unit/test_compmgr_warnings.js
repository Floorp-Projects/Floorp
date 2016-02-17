Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

function info(s) {
  dump("TEST-INFO | test_compmgr_warnings.js | " + s + "\n");
}

var gMessagesExpected = [
  { line: 2, message: /Malformed CID/, found: false },
  { line: 6, message: /re-register/, found: false },
  { line: 9, message: /Could not/, found: false },
  { line: 2, message: /binary component twice/, found: false },
  { line: 3, message: /binary component twice/, found: false },
];

const kConsoleListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleListener]),

  observe: function listener_observe(message) {
    if (!(message instanceof Ci.nsIScriptError)) {
      info("Not a script error: " + message.message);
      return;
    }

    info("Script error... " + message.sourceName + ":" + message.lineNumber + ": " + message.errorMessage);
    for (let expected of gMessagesExpected) {
      if (message.lineNumber != expected.line)
        continue;

      if (!expected.message.test(message.errorMessage))
        continue;

      info("Found expected message: " + expected.message);
      do_check_false(expected.found);
                
      expected.found = true;
    }
  }
};

function run_deferred_event(fn) {
  do_test_pending();
  Components.classes["@mozilla.org/thread-manager;1"].
    getService(Ci.nsIThreadManager).mainThread.dispatch(function() {
      fn();
      do_test_finished();
    }, 0);
}

function run_test()
{
  let cs = Components.classes["@mozilla.org/consoleservice;1"].
    getService(Ci.nsIConsoleService);
  cs.registerListener(kConsoleListener);

  var manifest = do_get_file('compmgr_warnings.manifest');
  registerAppManifest(manifest);
  manifest = do_get_file('components/testcomponent.manifest');
  registerAppManifest(manifest);

  run_deferred_event(function() {
    cs.unregisterListener(kConsoleListener);

    for (let expected of gMessagesExpected) {
      info("checking " + expected.message);
      do_check_true(expected.found);
    }
  });
}
