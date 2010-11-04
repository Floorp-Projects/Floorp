/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/AddonUpdateChecker.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

const RELATIVE_DIR = "browser/mobile/chrome/";
const TESTROOT     = "http://example.com/" + RELATIVE_DIR;
const TESTROOT2    = "http://example.org/" + RELATIVE_DIR;
const PREF_LOGGING_ENABLED            = "extensions.logging.enabled";
const PREF_SEARCH_MAXRESULTS          = "extensions.getAddons.maxResults";
const CHROME_NAME                     = "mochikit";
const PREF_AUTOUPDATE_DEFAULT         = "extensions.update.autoUpdateDefault"
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";
const PREF_GETADDONS_GETRECOMMENDED   = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const SEARCH_URL = TESTROOT + "browser_details.xml";

var addons = [{
  id: "addon1@tests.mozilla.org",
  name : "Install Tests",
  iconURL: "http://example.com/icon.png",
  homepageURL: "http://example.com/",
  version: "1.0",
  description: "Test add-on",
  sourceURL: TESTROOT + "addons/browser_install1_1.xpi"
},
{
  id: "addon2@tests.mozilla.org",
  name : "Install Tests 2",
  iconURL: "http://example.com/icon.png",
  homepageURL: "http://example.com/",
  version: "1.0",
  description: "Test add-on 2",
  sourceURL: TESTROOT + "addons/browser_install1_2.xpi"
}];


var gPendingTests = [];
var gTestsRun = 0;
var gTestStart = null;
var gDate = new Date(2010, 7, 1);
var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gCategoryUtilities;
var gSearchCount = 0;
var gProvider = null;

function test() {
  waitForExplicitFinish();
  Services.prefs.setCharPref(PREF_GETADDONS_GETRECOMMENDED,      TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSERECOMMENDED,   TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSESEARCHRESULTS, TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS,    TESTROOT + "browser_install.xml");
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
  run_next_test();
}

function end_test() {
  close_manager(function() {
    Services.prefs.clearUserPref(PREF_GETADDONS_GETRECOMMENDED);
    Services.prefs.clearUserPref(PREF_GETADDONS_BROWSERECOMMENDED);
    Services.prefs.clearUserPref(PREF_GETADDONS_GETSEARCHRESULTS);
    Services.prefs.clearUserPref(PREF_GETADDONS_BROWSESEARCHRESULTS);
    finish();
  });
}

function add_test(test) {
  gPendingTests.push(test);
}

function run_next_test() {
  if (gTestsRun > 0)
    info("Test " + gTestsRun + " took " + (Date.now() - gTestStart) + "ms");

  if (gPendingTests.length == 0) {
    end_test();
    return;
  }

  gTestsRun++;
  var test = gPendingTests.shift();
  if (test.name)
    info("Running test " + gTestsRun + " (" + test.name + ")");
  else
    info("Running test " + gTestsRun);

  gTestStart = Date.now();
  test();
}

function checkAttribute(aElt, aAttr, aVal) {
  ok(aElt.hasAttribute(aAttr), "Element has " + aAttr + " attribute");
  if(aVal)
    is(aElt.getAttribute(aAttr), aVal, "Element has " + aAttr + " attribute with value " + aVal);
}

function installExtension(elt, aListener) {
  elt.parentNode.ensureElementIsVisible(elt);
  elt.install.addListener(aListener)

  var button = document.getAnonymousElementByAttribute(elt, "class", "addon-install hide-on-install hide-on-restart");
  ok(!!button, "Extension has install button");
  ExtensionsView.installFromRepo(elt);
}

function isRestartShown(aShown, isUpdate) {
  let msg = document.getElementById("addons-messages");
  ok(!!msg, "Have message box");
  let notification = msg.getNotificationWithValue("restart-app");
  is(!!notification, aShown, "Restart exists = " + aShown);

  if(notification) {
    let label = "";
    dump("Label: " + notification.label + "\n");
    if(isUpdate)
      label = "Add-ons updated. Restart to complete changes."
    else
      label = "Restart to complete changes.";
    is(notification.label, label, "Restart shows correct message");
  }
  msg.removeAllNotifications(true)
}

function checkAddonListing(aAddon, elt) {
  ok(!!elt, "Element exists for addon");
  checkAttribute(elt, "id", "urn:mozilla:item:" + aAddon.id);
  checkAttribute(elt, "addonID", aAddon.id);
  checkAttribute(elt, "typeName", "search");
  checkAttribute(elt, "name", aAddon.name);
  checkAttribute(elt, "version", aAddon.version);
  checkAttribute(elt, "iconURL", aAddon.iconURL);
  checkAttribute(elt, "description", aAddon.description)
  checkAttribute(elt, "homepageURL", aAddon.homepageURL);
  checkAttribute(elt, "sourceURL", aAddon.sourceURL);
  ok(elt.install, "Extension has install property");
}
function checkUpdate(aAddon, aSettings) {
  let os = Services.obs;
  let ul = new updateListener(aSettings);
  os.addObserver(ul, "addon-update-ended", false);

  ExtensionsView.updateAll();
  //aAddon.findUpdates(new updateListener(aSettings), AddonManager.UPDATE_WHEN_USER_REQUESTED, "4.0", "4.0");
}

function get_addon_element(aId) {

  var node = document.getElementById("addons-list").firstChild;
  while (node) {
    if (("urn:mozilla:item:" + aId) == node.id)
      return node;
    node = node.nextSibling;
  }
  return null;

}

function open_manager(aView, aCallback) {
  var panelButton = document.getElementById("tool-panel-open")
  panelButton.click();
  var addonsButton = document.getElementById("tool-addons");
  addonsButton.click();

  if (!ExtensionsView._list) {
    window.addEventListener("ViewChanged", function() {
      window.removeEventListener("ViewChanged", arguments.callee, true);
      aCallback();
    }, true);
    
    ExtensionsView.init();
    ExtensionsView._delayedInit();
  } else {
    aCallback();
  }
}

function close_manager(aCallback) {
  var prefsButton = document.getElementById("tool-preferences");
  prefsButton.click();

  BrowserUI.hidePanel();
  aCallback();
}
// Installs a bootstrapped (restartless) addon first, and then
// updates it to a non-bootstrapped addon. Checks to make sure
// restart notifications are shown at the right time

// We currently don't handle bootstrapped addons very well, i.e.
// they aren't moved from the browse area into the main area
add_test(function() {
  open_manager(null, function() {
    var elt = get_addon_element("addon1@tests.mozilla.org");
    checkAddonListing(addons[0], elt);
    installExtension(elt, new installListener({
      showRestart: false,
      willFail:    false,
      onComplete:  function(aAddon) {
        checkUpdate(aAddon, {
          showRestart: true,
          willFail:    false,
          addon: addons[0],
          onComplete:  function(aAddon) {
            close_manager(run_next_test);
          }
        });
      }
    }));
  });
});


// Installs a non-bootstrapped addon and checks to make sure
// the correct restart notifications are shown
add_test(function() {
  open_manager(null, function() {
    var elt = get_addon_element("addon2@tests.mozilla.org");
    checkAddonListing(addons[1], elt);
    installExtension(elt, new installListener({
      showRestart: true,
      willFail:    false,
      onComplete:  run_next_test,
    }));
  });
})

function installListener(aSettings) {
  this.willFail    = aSettings.willFail;
  this.onComplete  = aSettings.onComplete;
  this.showRestart = aSettings.showRestart;
}

installListener.prototype = {

  onNewInstall : function(install) { },
  onDownloadStarted : function(install) {
    info("download started");
  },
  onDownloadProgress : function(install) {
    info("download progress");
  },
  onDownloadEnded : function(install) {
    info("download ended");
  },
  onDownloadCancelled : function(install) {
    info("download cancelled");
  },
  onDownloadFailed : function(install) {
    if(this.willFail)
      ok(false, "Install failed");
    info("download failed");
  },
  onInstallStarted : function(install) {
    info("Install started");
  },
  onInstallEnded : function(install, addon) {
    // this needs to fire after the extension manager's install ended
    let self = this;
    setTimeout(function() {
      info("Install ended");
      isRestartShown(self.showRestart, false);
      if(self.onComplete)
        self.onComplete(addon);
    }, 0);
  },
  onInstallCancelled : function(install) {
    info("Install cancelled");
  },
  onInstallFailed : function(install) {
    if(this.willFail)
      ok(false, "Install failed");
    info("install failed");
  },
  onExternalInstall : function(install, existing, needsRestart) { },
};

function updateListener(aSettings) {
  this.willFail = aSettings.willFail;
  this.onComplete = aSettings.onComplete;
  this.showRestart = aSettings.showRestart;
  this.addon = aSettings.addon;
}

updateListener.prototype = {
  observe: function (aSubject, aTopic, aData) {
    switch(aTopic) {
      case "addon-update-ended" :
        info("Update ended");
        // this needs to fire after the extension manager's install ended
        let self = this;
        
        let json = aSubject.QueryInterface(Ci.nsISupportsString).data;
        let update = JSON.parse(json);
        if(update.id == this.addon.id) {
          let element = get_addon_element(update.id);
          ok(!!element, "Have element for upgrade");
  
          let addon = element.addon;
  
          setTimeout(function() {
            isRestartShown(self.showRestart, true);
            if(self.onComplete)
              self.onComplete(addon);
          }, 100);
          let os = Services.obs;
          os.removeObserver(this, "addon-update-ended", false);
        }
        break;
    }
  },
}



