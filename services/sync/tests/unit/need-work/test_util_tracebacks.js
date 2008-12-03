Cu.import("resource://weave/util.js");

function _reportException(e) {
  dump("Exception caught.\n");
  dump("Exception: " + Utils.exceptionStr(e) + "\n");
  dump("Traceback:\n\n" + Utils.stackTrace(e) + "\n");
}

function test_stackTrace_works_with_error_object() {
  try {
    dump("Throwing new Error object.\n");
    throw new Error("Error!");
  } catch (e) {
    _reportException(e);
  }
}

function test_stackTrace_works_with_bare_string() {
  try {
    dump("Throwing bare string.\n");
    throw "Error!";
  } catch (e) {
    _reportException(e);
  }
}

function test_stackTrace_works_with_nsIException() {
  try {
    dump("Throwing a wrapped nsIException.\n");
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    dirSvc.get("nonexistentPropertyName");
  } catch (e) {
    _reportException(e);
  }
}
