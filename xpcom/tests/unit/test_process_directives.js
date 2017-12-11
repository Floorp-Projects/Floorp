Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  Components.manager.autoRegister(do_get_file("data/process_directive.manifest"));

  let isChild = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

  if (isChild) {
    do_check_false("@mozilla.org/xpcom/tests/MainProcessDirectiveTest;1" in Cc);
  } else {
    let svc = Cc["@mozilla.org/xpcom/tests/MainProcessDirectiveTest;1"].createInstance(Ci.nsISupportsString);
    do_check_eq(svc.data, "main process");
  }

  if (!isChild) {
    do_check_false("@mozilla.org/xpcom/tests/ChildProcessDirectiveTest;1" in Cc);
  } else {
    let svc = Cc["@mozilla.org/xpcom/tests/ChildProcessDirectiveTest;1"].createInstance(Ci.nsISupportsString);
    do_check_eq(svc.data, "child process");
  }
}
