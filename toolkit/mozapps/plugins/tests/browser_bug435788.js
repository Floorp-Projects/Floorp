const Cc = Components.classes;
const Ci = Components.interfaces;

const TEST_ROOT = "http://example.com/browser/toolkit/mozapps/plugins/tests/";

Components.utils.import("resource://gre/modules/AddonManager.jsm");

var gPrefs, gPFS, gDS, gSeenAvailable;

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

// Gets the number of plugin items in the detected list
function getListCount() {
  var list = gPFS.document.getElementById("pluginList");
  return list.childNodes.length;
}

// Gets wether the list contains a particular plugin name
function hasListItem(name, version) {
  var label = name + " " + (version ? version : "");
  var list = gPFS.document.getElementById("pluginList");
  for (var i = 0; i < list.childNodes.length; i++) {
    if (list.childNodes[i].label == label)
      return true;
  }
  return false;
}

// Gets the number of plugin results
function getResultCount() {
  var list = gPFS.document.getElementById("pluginResultList");
  return list.childNodes.length;
}

// Gets the plugin result for a particular plugin name
function getResultItem(name, version) {
  var label = name + " " + (version ? version : "");
  var list = gPFS.document.getElementById("pluginResultList");
  for (var i = 0; i < list.childNodes.length; i++) {
    if (list.childNodes[i].childNodes[1].value == label) {
      var item = {
        name: name,
        version: version,
        status: null
      };
      if (list.childNodes[i].childNodes[2].tagName == "label")
        item.status = list.childNodes[i].childNodes[2].value;
      return item;
    }
  }
  return null;
}

// Logs the currently displaying wizard page
function page_shown() {
  function show_button_state(name) {
    var button = gPFS.document.documentElement.getButton(name);
    info("Button " + name + ". hidden: " + button.hidden +
         ", disabled: " + button.disabled);
  }

  info("Page shown: " +
       gPFS.document.documentElement.currentPage.getAttribute("label"));
  show_button_state("next");
  show_button_state("finish");
}

function pfs_loaded() {
  info("PFS loaded");
  gPFS.document.documentElement.addEventListener("pageshow", page_shown, false);
  gPFS.document.documentElement.addEventListener("wizardfinish", function() {
    info("wizardfinish event");
  }, false);
  gPFS.document.documentElement.addEventListener("wizardnext", function() {
    info("wizardnext event");
  }, false);
  gPFS.addEventListener("unload", function() {
    info("unload event");
  }, false);
  page_shown();
}

// Test a working installer
function prepare_test_1() {
  ok(true, "Test 1");
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
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_2, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_1_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_1_complete);
  }, false);
}

function test_1_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 1", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_1_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test a broken installer (returns exit code 1)
function prepare_test_2() {
  ok(true, "Test 2");
  var missingPluginsArray = {
    "application/x-broken-installer": {
      mimetype: "application/x-broken-installer",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_2_start, false);
}

function test_2_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_3, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_2_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_2_complete);
  }, false);
}

function test_2_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 2", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_2_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test both working and broken together
function prepare_test_3() {
  ok(true, "Test 3");
  var missingPluginsArray = {
    "application/x-working-plugin": {
      mimetype: "application/x-working-plugin",
      pluginsPage: ""
    },
    "application/x-broken-installer": {
      mimetype: "application/x-broken-installer",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_3_start, false);
}

function test_3_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_4, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_3_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_3_complete);
  }, false);
}

function test_3_available() {
  gSeenAvailable = true;
  is(getListCount(), 2, "Should have found 2 plugins to install");
  ok(hasListItem("Test plugin 1", null), "Should have seen the right plugin name");
  ok(hasListItem("Test plugin 2", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_3_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 2, "Should have attempted to install 2 plugins");
  var item = getResultItem("Test plugin 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");
  item = getResultItem("Test plugin 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test an installer with a bad hash
function prepare_test_4() {
  ok(true, "Test 4");
  var missingPluginsArray = {
    "application/x-broken-plugin-hash": {
      mimetype: "application/x-broken-plugin-hash",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_4_start, false);
}

function test_4_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_5, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_4_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_4_complete);
  }, false);
}

function test_4_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 3", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_4_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 3", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have not been a successful install");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test a working xpi
function prepare_test_5() {
  ok(true, "Test 5");

  var missingPluginsArray = {
    "application/x-working-extension": {
      mimetype: "application/x-working-extension",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_5_start, false);
}

function test_5_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_6, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_5_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_5_complete);
  }, false);
}

function test_5_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 1", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_5_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");

  AddonManager.getAllInstalls(function(installs) {
    is(installs.length, 1, "Should be just one install");
    is(installs[0].state, AddonManager.STATE_INSTALLED, "Should be fully installed");
    is(installs[0].addon.id, "bug435788_1@tests.mozilla.org", "Should have installed the extension");
    installs[0].cancel();

    var finish = gPFS.document.documentElement.getButton("finish");
    ok(!finish.hidden, "Finish button should not be hidden");
    ok(!finish.disabled, "Finish button should not be disabled");
    finish.click();
  });
}

// Test a broke xpi (no install.rdf)
function prepare_test_6() {
  ok(true, "Test 6");
  var missingPluginsArray = {
    "application/x-broken-extension": {
      mimetype: "application/x-broken-extension",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_6_start, false);
}

function test_6_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_7, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_6_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_6_complete);
  }, false);
}

function test_6_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 2", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_6_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test both working and broken xpi
function prepare_test_7() {
  ok(true, "Test 7");
  var missingPluginsArray = {
    "application/x-working-extension": {
      mimetype: "application/x-working-extension",
      pluginsPage: ""
    },
    "application/x-broken-extension": {
      mimetype: "application/x-broken-extension",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_7_start, false);
}

function test_7_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_8, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_7_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_7_complete);
  }, false);
}

function test_7_available() {
  gSeenAvailable = true;
  is(getListCount(), 2, "Should have found 2 plugins to install");
  ok(hasListItem("Test extension 1", null), "Should have seen the right plugin name");
  ok(hasListItem("Test extension 2", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_7_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 2, "Should have attempted to install 2 plugins");
  var item = getResultItem("Test extension 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a failed install");
  item = getResultItem("Test extension 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  AddonManager.getAllInstalls(function(installs) {
    is(installs.length, 1, "Should be one active installs");
    installs[0].cancel();

    gPFS.document.documentElement.getButton("finish").click();
  });

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test an xpi with a bad hash
function prepare_test_8() {
  ok(true, "Test 8");
  var missingPluginsArray = {
    "application/x-broken-extension-hash": {
      mimetype: "application/x-broken-extension-hash",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_8_start, false);
}

function test_8_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_9, false);
  gSeenAvailable = false;

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_8_available);
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_8_complete);
  }, false);
}

function test_8_available() {
  gSeenAvailable = true;
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 3", null), "Should have seen the right plugin name");

  gPFS.document.documentElement.getButton("next").click();
}

function test_8_complete() {
  ok(gSeenAvailable, "Should have seen the list of available plugins");
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 3", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have not been a successful install");

  AddonManager.getAllInstalls(function(installs) {
    is(installs.length, 0, "Should not be any installs");

    gPFS.document.documentElement.getButton("finish").click();
  });

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test when no plugin exists in the datasource
function prepare_test_9() {
  ok(true, "Test 9");
  var missingPluginsArray = {
    "application/x-unknown-plugin": {
      mimetype: "application/x-unknown-plugin",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_9_start, false);
}

function test_9_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_10, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    ok(false, "Should not have found plugins to install");
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_9_complete);
  }, false);
}

function test_9_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test when the datasource is invalid xml
function prepare_test_10() {
  ok(true, "Test 10");
  gPrefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_2.rdf");

  var missingPluginsArray = {
    "application/x-broken-xml": {
      mimetype: "application/x-broken-xml",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_10_start, false);
}

function test_10_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", prepare_test_11, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    ok(false, "Should not have found plugins to install");
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_10_complete);
  }, false);
}

function test_10_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test when no datasource is returned
function prepare_test_11() {
  ok(true, "Test 11");
  gPrefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_foo.rdf");

  var missingPluginsArray = {
    "application/x-missing-xml": {
      mimetype: "application/x-missing-xml",
      pluginsPage: ""
    }
  };

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});
  gPFS.addEventListener("load", test_11_start, false);
}

function test_11_start() {
  pfs_loaded();
  gPFS.addEventListener("unload", finishTest, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    ok(false, "Should not have found plugins to install");
  }, false);
  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_11_complete);
  }, false);
}

function test_11_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}
