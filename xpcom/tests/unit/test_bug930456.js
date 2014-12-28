const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource:///modules/Services.jsm");

function run_test()
{
  let isChild = Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

  if (isChild) {
    do_check_false("@mozilla.org/browser/search-service;1" in Cc);
  } else {
    do_check_true("@mozilla.org/browser/search-service;1" in Cc);
  }
}
