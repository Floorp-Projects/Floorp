/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/async.js");
Cu.import("resource://testing-common/services/common/utils.js");

let provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    switch (prop) {
      case "ExtPrefDL":
        return [Services.dirsvc.get("CurProcD", Ci.nsIFile)];
      default:
        throw Cr.NS_ERROR_FAILURE;
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
};
Services.dirsvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

// This is needed for loadAddonTestFunctions().
let gGlobalScope = this;

function ExtensionsTestPath(path) {
  if (path[0] != "/") {
    throw Error("Path must begin with '/': " + path);
  }

  return "../../../../toolkit/mozapps/extensions/test/xpcshell" + path;
}

/**
 * Loads the AddonManager test functions by importing its test file.
 *
 * This should be called in the global scope of any test file needing to
 * interface with the AddonManager. It should only be called once, or the
 * universe will end.
 */
function loadAddonTestFunctions() {
  const path = ExtensionsTestPath("/head_addons.js");
  let file = do_get_file(path);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec, gGlobalScope);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
}

function getAddonInstall(name) {
  let f = do_get_file(ExtensionsTestPath("/addons/" + name + ".xpi"));
  let cb = Async.makeSyncCallback();
  AddonManager.getInstallForFile(f, cb);

  return Async.waitForSyncCallback(cb);
}

/**
 * Obtains an addon from the add-on manager by id.
 *
 * This is merely a synchronous wrapper.
 *
 * @param  id
 *         ID of add-on to fetch
 * @return addon object on success or undefined or null on failure
 */
function getAddonFromAddonManagerByID(id) {
   let cb = Async.makeSyncCallback();
   AddonManager.getAddonByID(id, cb);
   return Async.waitForSyncCallback(cb);
}

/**
 * Installs an add-on synchronously from an addonInstall
 *
 * @param  install addonInstall instance to install
 */
function installAddonFromInstall(install) {
  let cb = Async.makeSyncCallback();
  let listener = {onInstallEnded: cb};
  AddonManager.addInstallListener(listener);
  install.install();
  Async.waitForSyncCallback(cb);
  AddonManager.removeAddonListener(listener);

  do_check_neq(null, install.addon);
  do_check_neq(null, install.addon.syncGUID);

  return install.addon;
}

/**
 * Convenience function to install an add-on from the extensions unit tests.
 *
 * @param  name
 *         String name of add-on to install. e.g. test_install1
 * @return addon object that was installed
 */
function installAddon(name) {
  let install = getAddonInstall(name);
  do_check_neq(null, install);
  return installAddonFromInstall(install);
}

/**
 * Convenience function to uninstall an add-on synchronously.
 *
 * @param addon
 *        Addon instance to uninstall
 */
function uninstallAddon(addon) {
  let cb = Async.makeSyncCallback();
  let listener = {onUninstalled: function(uninstalled) {
    if (uninstalled.id == addon.id) {
      AddonManager.removeAddonListener(listener);
      cb(uninstalled);
    }
  }};

  AddonManager.addAddonListener(listener);
  addon.uninstall();
  Async.waitForSyncCallback(cb);
}

function generateNewKeys(collectionKeys, collections=null) {
  let wbo = collectionKeys.generateNewKeysWBO(collections);
  let modified = new_timestamp();
  collectionKeys.setContents(wbo.cleartext, modified);
}

// Helpers for testing open tabs.
// These reflect part of the internal structure of TabEngine,
// and stub part of Service.wm.

function mockShouldSkipWindow (win) {
  return win.closed ||
         win.mockIsPrivate;
}

function mockGetTabState (tab) {
  return tab;
}

function mockGetWindowEnumerator(url, numWindows, numTabs) {
  let elements = [];
  for (let w = 0; w < numWindows; ++w) {
    let tabs = [];
    let win = {
      closed: false,
      mockIsPrivate: false,
      gBrowser: {
        tabs: tabs,
      },
    };
    elements.push(win);

    for (let t = 0; t < numTabs; ++t) {
      tabs.push(TestingUtils.deepCopy({
        index: 1,
        entries: [{
          url: ((typeof url == "string") ? url : url()),
          title: "title"
        }],
        attributes: {
          image: "image"
        },
        lastAccessed: 1499
      }));
    }
  }

  // Always include a closed window and a private window.
  elements.push({
    closed: true,
    mockIsPrivate: false,
    gBrowser: {
      tabs: [],
    },
  });
 
  elements.push({
    closed: false,
    mockIsPrivate: true,
    gBrowser: {
      tabs: [],
    },
  });

  return {
    hasMoreElements: function () {
      return elements.length;
    },
    getNext: function () {
      return elements.shift();
    },
  };
}

// Helper that allows checking array equality.
function do_check_array_eq(a1, a2) {
  do_check_eq(a1.length, a2.length);
  for (let i = 0; i < a1.length; ++i) {
    do_check_eq(a1[i], a2[i]);
  }
}
