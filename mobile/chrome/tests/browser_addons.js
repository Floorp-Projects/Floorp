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
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_UPDATE              = "extensions.update.url";
const SEARCH_URL = TESTROOT + "browser_details.xml";

var addons = [{
  id: "addon1@tests.mozilla.org",
  name : "Install Tests",
  iconURL: "http://example.com/icon.png",
  homepageURL: "http://example.com/",
  version: "1.0",
  description: "Test add-on",
  sourceURL: TESTROOT + "addons/browser_install1_1.xpi",
  bootstrapped: true,
  willFail: false,
  updateIndex: 2,
},
{
  id: "addon2@tests.mozilla.org",
  name : "Install Tests 2",
  iconURL: "http://example.com/icon.png",
  homepageURL: "http://example.com/",
  version: "1.0",
  description: "Test add-on 2",
  sourceURL: TESTROOT + "addons/browser_install1_2.xpi",
  bootstrapped: false,
  willFail: false,
},
{
  id: "addon1@tests.mozilla.org",
  name : "Install Tests 3",
  iconURL: "http://example.com/icon.png",
  homepageURL: "http://example.com/",
  version: "1.0",
  description: "Test add-on 3",
  sourceURL: TESTROOT + "addons/browser_install1_3.xpi",
  bootstrapped: false,
  willFail: false,
}];


var gPendingTests = [];
var gTestsRun = 0;
var gTestStart = null;
var gDate = new Date(2010, 7, 1);
var gApp = Strings.brand.GetStringFromName("brandShortName");
var gCategoryUtilities;
var gSearchCount = 0;
var gProvider = null;

function test() {
  waitForExplicitFinish();
  Services.prefs.setCharPref(PREF_GETADDONS_GETRECOMMENDED,      TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSERECOMMENDED,   TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSESEARCHRESULTS, TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS,    TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_UPDATE,              TESTROOT + "browser_upgrade.rdf");
  Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
  run_next_test();
}

function end_test() {
  close_manager();
  Services.prefs.clearUserPref(PREF_GETADDONS_GETRECOMMENDED);
  Services.prefs.clearUserPref(PREF_GETADDONS_BROWSERECOMMENDED);
  Services.prefs.clearUserPref(PREF_GETADDONS_GETSEARCHRESULTS);
  Services.prefs.clearUserPref(PREF_GETADDONS_BROWSESEARCHRESULTS);
  finish();
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

function isRestartShown(aShown, isUpdate, aCallback) {
  let msg = document.getElementById("addons-messages");
  ok(!!msg, "Have message box");

  let done = function(aNotification) {
    is(!!aNotification, aShown, "Restart exists = " + aShown);
    if (aShown && aNotification) {
      let showsUpdate = aNotification.label.match(/update/i) != null;
      // this test regularly fails due to race conditions here
      is(showsUpdate, isUpdate, "Restart shows correct message");
    }
    msg.removeAllNotifications(true);
    aCallback();
  }

  let notification = msg.getNotificationWithValue("restart-app");
  if (!notification && aShown) {
    window.addEventListener("AlertActive", function() {
      window.removeEventListener("AlertActive", arguments.callee, true);
      notification = msg.getNotificationWithValue("restart-app");
      done(notification);
    }, true);
  } else {
    done(notification);
  }
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
function checkUpdate(aSettings) {
  let os = Services.obs;
  let ul = new updateListener(aSettings);
  os.addObserver(ul, "addon-update-ended", false);

  ExtensionsView.updateAll();
}

function get_addon_element(aId) {
  return document.getElementById("urn:mozilla:item:" + aId);
}

function open_manager(aView, aCallback) {
  BrowserUI.showPanel("addons-container");

  ExtensionsView.init();
  ExtensionsView._delayedInit();

  window.addEventListener("ViewChanged", function() {
    window.removeEventListener("ViewChanged", arguments.callee, true);
     aCallback();
  }, true);
}

function close_manager() {
  var prefsButton = document.getElementById("tool-preferences");
  prefsButton.click();
 
  ExtensionsView.clearSection();
  ExtensionsView.clearSection("local");
  ExtensionsView._list = null;
  ExtensionsView._restartCount = 0;
  BrowserUI.hidePanel();
}

// Installs an addon from the addons pref pane, and then
// updates it if requested. Checks to make sure
// restart notifications are shown at the right time
function installFromAddonsPage(aAddon, aDoUpdate) {
  return function() {
    open_manager(null, function() {
      var elt = get_addon_element(aAddon.id);
      checkAddonListing(aAddon, elt);
      installExtension(elt, new installListener({
        addon: aAddon,
        onComplete:  function() {
          if (aDoUpdate) {
            checkUpdate({
              addon: addons[aAddon.updateIndex],
              onComplete:  function() {
                close_manager();
                run_next_test();
              }
            });
          } else {
            close_manager();
            run_next_test();
          }
        }
      }));
    });
  }
}

add_test(installFromAddonsPage(addons[0], true));
add_test(installFromAddonsPage(addons[1], false));

function installListener(aSettings) {
  this.onComplete = aSettings.onComplete;
  this.addon = aSettings.addon;
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
    if(this.addon.willFail)
      ok(false, "Install failed");
    info("download failed");
  },
  onInstallStarted : function(install) {
    info("Install started");
  },
  onInstallEnded : function(install, addon) {
    info("Install ended");
    let self = this;
    isRestartShown(!this.addon.bootstrapped, false, function() {
      if(self.onComplete)
        self.onComplete();
    });
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
  this.onComplete = aSettings.onComplete;
  this.addon = aSettings.addon;
}

updateListener.prototype = {
  observe: function (aSubject, aTopic, aData) {
    switch(aTopic) {
      case "addon-update-ended" :
        let json = aSubject.QueryInterface(Ci.nsISupportsString).data;
        let update = JSON.parse(json);
        if(update.id == this.addon.id) {
          let os = Services.obs;
          os.removeObserver(this, "addon-update-ended", false);

          let element = get_addon_element(update.id);
          ok(!!element, "Have element for upgrade");  

          let self = this;
          isRestartShown(!this.addon.bootstrapped, true, function() {
            if(self.onComplete)
              self.onComplete();
          });
        }
        break;
    }
  },
}



