Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/util.js");


// See toolkit/components/places/tests/head_common.js
const NS_APP_HISTORY_50_FILE = "UHist";
const dirsvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIDirectoryService);

// Add our own dirprovider for old history.dat.
let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == NS_APP_HISTORY_50_FILE) {
      let histFile = dirsvc.get("ProfD", Ci.nsIFile);
      histFile.append("history.dat");
      return histFile;
    }
    throw Cr.NS_ERROR_FAILURE;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
};
dirsvc.registerProvider(provider);


function run_test() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = new HistoryEngine()._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let _counter = 0;
  function addVisit() {
    Svc.History.addVisit(Utils.makeURI("http://getfirefox.com/" + _counter),
                         Date.now() * 1000, null, 1, false, 0);
    _counter += 1;
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 1);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  } finally {
    _("Clean up.");
    Svc.History.removeAllPages();
  }
}
