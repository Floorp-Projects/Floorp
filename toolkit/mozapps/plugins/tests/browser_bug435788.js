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
  gPFS.addEventListener("unload", prepare_test_2, false);

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

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_3, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_2_available);
  }, false);
}

function test_2_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test plugin 2 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_2_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_2_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have been a failed install");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_4, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_3_available);
  }, false);
}

function test_3_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 2, "Should have found 2 plugins to install");
  is(list.childNodes[0].label, "Test plugin 1 ", "Should have seen the right plugin name");
  is(list.childNodes[1].label, "Test plugin 2 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_3_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_3_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 2, "Should have attempted to install 2 plugins");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Installed", "Should have been a failed install");
  status = list.childNodes[1].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have been a failed install");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_5, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_4_available);
  }, false);
}

function test_4_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test plugin 3 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_4_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_4_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have not been a successful install");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_6, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_5_available);
  }, false);
}

function test_5_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test extension 1 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_5_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_5_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Installed", "Should have been a successful install");

  var em = Cc["@mozilla.org/extensions/manager;1"].
           getService(Ci.nsIExtensionManager);
  ok(em.getItemForID("bug435788_1@tests.mozilla.org"), "Should have installed the extension");
  em.cancelInstallItem("bug435788_1@tests.mozilla.org");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_7, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_6_available);
  }, false);
}

function test_6_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test extension 2 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_6_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_6_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have been a failed install");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_8, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_7_available);
  }, false);
}

function test_7_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 2, "Should have found 2 plugins to install");
  is(list.childNodes[0].label, "Test extension 1 ", "Should have seen the right plugin name");
  is(list.childNodes[1].label, "Test extension 2 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_7_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_7_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 2, "Should have attempted to install 2 plugins");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Installed", "Should have been a failed install");
  status = list.childNodes[1].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have been a failed install");

  var em = Cc["@mozilla.org/extensions/manager;1"].
           getService(Ci.nsIExtensionManager);
  ok(em.getItemForID("bug435788_1@tests.mozilla.org"), "Should have installed the extension");
  em.cancelInstallItem("bug435788_1@tests.mozilla.org");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_9, false);

  gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", function() {
    executeSoon(test_8_available);
  }, false);
}

function test_8_available() {
  var list = gPFS.document.getElementById("pluginList");
  is(list.childNodes.length, 1, "Should have found 1 plugin to install");
  is(list.childNodes[0].label, "Test extension 3 ", "Should have seen the right plugin name");

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_8_complete);
  }, false);
  gPFS.document.documentElement.getButton("next").click();
}

function test_8_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 1, "Should have attempted to install 1 plugin");
  var status = list.childNodes[0].childNodes[2];
  is(status.tagName, "label", "Should have a status");
  is(status.value, "Failed", "Should have not been a successful install");

  var em = Cc["@mozilla.org/extensions/manager;1"].
           getService(Ci.nsIExtensionManager);
  ok(!em.getItemForID("bug435788_1@tests.mozilla.org"), "Should not have installed the extension");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_10, false);

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_9_complete);
  }, false);
}

function test_9_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 0, "Should have found no plugins");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", prepare_test_11, false);

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_10_complete);
  }, false);
}

function test_10_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 0, "Should have found no plugins");

  gPFS.document.documentElement.getButton("finish").click();
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
  gPFS.addEventListener("unload", finishTest, false);

  gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", function() {
    executeSoon(test_10_complete);
  }, false);
}

function test_11_complete() {
  var list = gPFS.document.getElementById("pluginResultList");
  is(list.childNodes.length, 0, "Should have found no plugins");

  gPFS.document.documentElement.getButton("finish").click();
}
