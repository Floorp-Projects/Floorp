const TEST_ROOT = "http://example.com/browser/toolkit/mozapps/plugins/tests/";

let tmp = {};
Components.utils.import("resource://gre/modules/AddonManager.jsm", tmp);
let AddonManager = tmp.AddonManager;

var gPFS;

function test() {
  waitForExplicitFinish();

  prepare_test_1();
}

function finishTest() {
  Services.prefs.clearUserPref("pfs.datasource.url");
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
  var docEle = gPFS.document.documentElement;

  var onwizardfinish = function () {
    info("wizardfinish event");
  };
  var onwizardnext = function () {
    info("wizardnext event");
  };

  docEle.addEventListener("pageshow", page_shown, false);
  docEle.addEventListener("wizardfinish", onwizardfinish, false);
  docEle.addEventListener("wizardnext", onwizardnext, false);

  gPFS.addEventListener("unload", function() {
    info("unload event");
    gPFS.removeEventListener("unload", arguments.callee, false);
    docEle.removeEventListener("pageshow", page_shown, false);
    docEle.removeEventListener("wizardfinish", onwizardfinish, false);
    docEle.removeEventListener("wizardnext", onwizardnext, false);
  }, false);

  page_shown();
}

function startTest(num, missingPluginsArray) {
  info("Test " + num);

  gPFS = window.openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                           "PFSWindow", "chrome,centerscreen,resizable=yes",
                           {plugins: missingPluginsArray});

  var testScope = this;

  gPFS.addEventListener("load", function () {
    gPFS.removeEventListener("load", arguments.callee, false);

    pfs_loaded();

    var seenAvailable = false;
    var expectAvailable = typeof testScope["test_" + num + "_available"] == "function";

    function availableListener() {
      seenAvailable = true;

      if (expectAvailable) {
        executeSoon(function () {
          testScope["test_" + num + "_available"]();
          gPFS.document.documentElement.getButton("next").click();
        });
      } else {
        ok(false, "Should not have found plugins to install");
      }
    }

    function completeListener() {
      if (expectAvailable)
        ok(seenAvailable, "Should have seen the list of available plugins");

      executeSoon(testScope["test_" + num + "_complete"]);
    }

    gPFS.document.documentElement.wizardPages[1].addEventListener("pageshow", availableListener);
    gPFS.document.documentElement.wizardPages[4].addEventListener("pageshow", completeListener);

    gPFS.addEventListener("unload", function () {
      gPFS.removeEventListener("unload", arguments.callee, false);
      gPFS.document.documentElement.wizardPages[1].removeEventListener("pageshow", availableListener, false);
      gPFS.document.documentElement.wizardPages[4].removeEventListener("pageshow", completeListener, false);

      num++;
      if (typeof testScope["prepare_test_" + num] == "function")
        testScope["prepare_test_" + num]();
      else
        finishTest();
    });
  });
}

function clickFinish() {
  var finish = gPFS.document.documentElement.getButton("finish");
  ok(!finish.hidden, "Finish button should not be hidden");
  ok(!finish.disabled, "Finish button should not be disabled");
  finish.click();
}

// Test a working installer
function prepare_test_1() {
  Services.prefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_1.rdf");

  var missingPluginsArray = {
    "application/x-working-plugin": {
      mimetype: "application/x-working-plugin",
      pluginsPage: ""
    }
  };

  startTest(1, missingPluginsArray);
}

function test_1_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 1", null), "Should have seen the right plugin name");
}

function test_1_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");

  clickFinish();
}

// Test a broken installer (returns exit code 1)
function prepare_test_2() {
  var missingPluginsArray = {
    "application/x-broken-installer": {
      mimetype: "application/x-broken-installer",
      pluginsPage: ""
    }
  };

  startTest(2, missingPluginsArray);
}

function test_2_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 2", null), "Should have seen the right plugin name");
}

function test_2_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  clickFinish();
}

// Test both working and broken together
function prepare_test_3() {
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

  startTest(3, missingPluginsArray);
}

function test_3_available() {
  is(getListCount(), 2, "Should have found 2 plugins to install");
  ok(hasListItem("Test plugin 1", null), "Should have seen the right plugin name");
  ok(hasListItem("Test plugin 2", null), "Should have seen the right plugin name");
}

function test_3_complete() {
  is(getResultCount(), 2, "Should have attempted to install 2 plugins");
  var item = getResultItem("Test plugin 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");
  item = getResultItem("Test plugin 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  clickFinish();
}

// Test an installer with a bad hash
function prepare_test_4() {
  var missingPluginsArray = {
    "application/x-broken-plugin-hash": {
      mimetype: "application/x-broken-plugin-hash",
      pluginsPage: ""
    }
  };

  startTest(4, missingPluginsArray);
}

function test_4_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test plugin 3", null), "Should have seen the right plugin name");
}

function test_4_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test plugin 3", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have not been a successful install");

  clickFinish();
}

// Test a working xpi
function prepare_test_5() {
  var missingPluginsArray = {
    "application/x-working-extension": {
      mimetype: "application/x-working-extension",
      pluginsPage: ""
    }
  };

  startTest(5, missingPluginsArray);
}

function test_5_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 1", null), "Should have seen the right plugin name");
}

function test_5_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 1", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Installed", "Should have been a successful install");

  AddonManager.getAllInstalls(function(installs) {
    is(installs.length, 1, "Should be just one install");
    is(installs[0].state, AddonManager.STATE_INSTALLED, "Should be fully installed");
    is(installs[0].addon.id, "bug435788_1@tests.mozilla.org", "Should have installed the extension");
    installs[0].cancel();

    clickFinish();
  });
}

// Test a broke xpi (no install.rdf)
function prepare_test_6() {
  var missingPluginsArray = {
    "application/x-broken-extension": {
      mimetype: "application/x-broken-extension",
      pluginsPage: ""
    }
  };

  startTest(6, missingPluginsArray);
}

function test_6_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 2", null), "Should have seen the right plugin name");
}

function test_6_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 2", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have been a failed install");

  clickFinish();
}

// Test both working and broken xpi
function prepare_test_7() {
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

  startTest(7, missingPluginsArray);
}

function test_7_available() {
  is(getListCount(), 2, "Should have found 2 plugins to install");
  ok(hasListItem("Test extension 1", null), "Should have seen the right plugin name");
  ok(hasListItem("Test extension 2", null), "Should have seen the right plugin name");
}

function test_7_complete() {
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

    clickFinish();
  });
}

// Test an xpi with a bad hash
function prepare_test_8() {
  var missingPluginsArray = {
    "application/x-broken-extension-hash": {
      mimetype: "application/x-broken-extension-hash",
      pluginsPage: ""
    }
  };

  startTest(8, missingPluginsArray);
}

function test_8_available() {
  is(getListCount(), 1, "Should have found 1 plugin to install");
  ok(hasListItem("Test extension 3", null), "Should have seen the right plugin name");
}

function test_8_complete() {
  is(getResultCount(), 1, "Should have attempted to install 1 plugin");
  var item = getResultItem("Test extension 3", null);
  ok(item, "Should have seen the installed item");
  is(item.status, "Failed", "Should have not been a successful install");

  AddonManager.getAllInstalls(function(installs) {
    is(installs.length, 0, "Should not be any installs");

    clickFinish();
  });
}

// Test when no plugin exists in the datasource
function prepare_test_9() {
  var missingPluginsArray = {
    "application/x-unknown-plugin": {
      mimetype: "application/x-unknown-plugin",
      pluginsPage: ""
    }
  };

  startTest(9, missingPluginsArray);
}

function test_9_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  clickFinish();
}

// Test when the datasource is invalid xml
function prepare_test_10() {
  Services.prefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_2.rdf");

  var missingPluginsArray = {
    "application/x-broken-xml": {
      mimetype: "application/x-broken-xml",
      pluginsPage: ""
    }
  };

  startTest(10, missingPluginsArray);
}

function test_10_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  clickFinish();
}

// Test when no datasource is returned
function prepare_test_11() {
  Services.prefs.setCharPref("pfs.datasource.url", TEST_ROOT + "pfs_bug435788_foo.rdf");

  var missingPluginsArray = {
    "application/x-missing-xml": {
      mimetype: "application/x-missing-xml",
      pluginsPage: ""
    }
  };

  startTest(11, missingPluginsArray);
}

function test_11_complete() {
  is(getResultCount(), 0, "Should have found no plugins");

  clickFinish();
}
