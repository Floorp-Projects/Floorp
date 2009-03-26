const Cc = Components.classes;
const Ci = Components.interfaces;

const TEST_ROOT = "http://example.com/browser/toolkit/mozapps/plugins/tests/";

var gPrefs, gPFS, gDS;

function test() {
  waitForExplicitFinish();

  gPrefs = Cc["@mozilla.org/preferences-service;1"].
           getService(Ci.nsIPrefBranch);
  gDS = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
  prepare_test_1();
}

function finishTest(e) {
  gPrefs.clearUserPref("pfs.datasource.url");
  finish();
}

function delete_installer() {
  // Guess at the filename for the installer and delete it
  try {
    var filename = "setup";
    if (Components.classes["@mozilla.org/xre/app-info;1"]
                  .getService(Components.interfaces.nsIXULRuntime)
                  .OS == "WINNT")
      filename += ".exe";
    var file = gDS.get("TmpD", Ci.nsIFile);
    file.append(filename);
    if (file.exists())
      file.remove(false);
  }
  catch (e) { }
}

function prepare_test_1() {
  gPrefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_1.rdf");

  var missingPluginsArray = {
    "application/x-working-plugin": {
      mimetype: "application/x-working-plugin",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_1_start, false);
}

function test_1_start() {
  gPFS.addEventListener("unload", finishTest, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_1_available);
  }, false);
}

function test_1_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test plugin 1 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_1_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_1_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Installed", "Should have been a successful install");

  // This is necessary because PFS doesn't delete the installer it runs. Give it
  // a second to finish then delete it ourselves
  setTimeout(test_1_finish, 1000);
}

function test_1_finish() {
  delete_installer();

  gPFS.document.documentElement.getButton("finish").click();
}
