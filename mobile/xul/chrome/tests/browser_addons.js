/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const RELATIVE_DIR = "browser/mobile/chrome/tests/";
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
const PREF_ADDONS_LOGGING                = "extensions.logging.enabled";
const PREF_ADDONS_SECURITY               = "extensions.checkUpdateSecurity";
const SEARCH_URL = TESTROOT + "browser_details.xml";
const ADDON_IMG = "chrome://browser/skin/images/alert-addons-30.png";

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
var gTestURL = TESTROOT + "browser_blank_01.html";
var gCurrentTab = null;

function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);
  Services.prefs.setCharPref(PREF_GETADDONS_GETRECOMMENDED,      TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSERECOMMENDED,   TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_BROWSESEARCHRESULTS, TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS,    TESTROOT + "browser_install.xml");
  Services.prefs.setCharPref(PREF_GETADDONS_UPDATE,              TESTROOT + "browser_upgrade.rdf");
  Services.prefs.setBoolPref(PREF_ADDONS_SECURITY, false);
  Services.prefs.setBoolPref(PREF_ADDONS_LOGGING, true);
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  run_next_test();
}

function end_test() {
  close_manager();
  Services.prefs.clearUserPref(PREF_GETADDONS_GETRECOMMENDED);
  Services.prefs.clearUserPref(PREF_GETADDONS_BROWSERECOMMENDED);
  Services.prefs.clearUserPref(PREF_GETADDONS_BROWSESEARCHRESULTS);
  Services.prefs.clearUserPref(PREF_GETADDONS_GETSEARCHRESULTS);
  Services.prefs.clearUserPref(PREF_GETADDONS_UPDATE);
  Services.prefs.clearUserPref(PREF_ADDONS_SECURITY);
  Services.prefs.clearUserPref(PREF_ADDONS_LOGGING);
  Services.prefs.clearUserPref(PREF_SEARCH_MAXRESULTS);
}

registerCleanupFunction(end_test);

function add_test(test) {
  gPendingTests.push(test);
}

function run_next_test() {
  if (gTestsRun > 0)
    info("Test " + gTestsRun + " took " + (Date.now() - gTestStart) + "ms");

   if (!gPendingTests.length) {
    finish();
    return;
  }

  gTestsRun++;
  let test = gPendingTests.shift();
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

  let button = document.getAnonymousElementByAttribute(elt, "class", "addon-install hide-on-install hide-on-restart");
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

function checkInstallAlert(aShown, aCallback) {
  checkAlert(null, "xpinstall", null, aShown, function(aNotifyBox, aNotification) {
    if (aShown) {
      let button = aNotification.childNodes[0];
      ok(!!button, "Notification has button");
      if (button)
        button.click();
    }
    aNotifyBox.removeAllNotifications(true);
    if (aCallback)
      aCallback();
  });
}

function checkDownloadNotification(aCallback) {
  let msg = /download/i;
  checkNotification(/Add-ons/, msg, ADDON_IMG, aCallback);
}

function checkInstallNotification(aRestart, aCallback) {
  let msg = null;
  if (aRestart)
    msg = /restart/i;
  checkNotification(/Add-ons/, msg, ADDON_IMG, aCallback);
}

function checkNotification(aTitle, aMessage, aIcon, aCallback) {
  let doTest = function() {
    ok(document.getElementById("alerts-container").classList.contains("showing"), "Alert shown");
    let title = document.getElementById("alerts-title").value;
    let msg   = document.getElementById("alerts-text").textContent;
    let img   = document.getElementById("alerts-image").getAttribute("src");

    if (aTitle)
      ok(aTitle.test(title), "Correct title alert shown: " + title);
    if (aMessage)
      ok(aMessage.test(msg), "Correct message shown: " + msg);
    if (aIcon)
      is(img, aIcon, "Correct image shown: " + aIcon);

    // make sure this is hidden before another test asks about it
    AlertsHelper.container.classList.remove("showing");
    AlertsHelper.container.height = 0;
    AlertsHelper.container.hidden = true;
    aCallback();
  };

  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  if (sysInfo.get("device"))
    aCallback();
  else
    waitFor(doTest, function() { return AlertsHelper.container.hidden == false; });
}

function checkAlert(aId, aName, aLabel, aShown, aCallback) {
  let msg = null;
  if (aId)
    msg = document.getElementById(aId);
  else
    msg = window.getNotificationBox(gCurrentTab.browser);
  ok(!!msg, "Have notification box");

  let haveNotification = function(notify) {
    is(!!notify, aShown, "Notification alert exists = " + aShown);
    if (notify && aLabel)
      ok(aLabel.test(notify.label), "Notification shows correct message");
    if (aCallback)
      aCallback(msg, notify);
  }

  let notification = msg.getNotificationWithValue(aName);
  if (!notification && aShown) {
    window.addEventListener("AlertActive", function() {
      window.removeEventListener("AlertActive", arguments.callee, true);
      notification = msg.getNotificationWithValue(aName);
      haveNotification(notification);
    }, true);
  } else {
    haveNotification(notification);
  }
}

function checkAddonListing(aAddon, elt, aType) {
  ok(!!elt, "Element exists for addon");
  checkAttribute(elt, "id", "urn:mozilla:item:" + aAddon.id);
  checkAttribute(elt, "addonID", aAddon.id);
  checkAttribute(elt, "typeName", aType);
  checkAttribute(elt, "name", aAddon.name);
  checkAttribute(elt, "version", aAddon.version);
  if (aType == "search") {
    checkAttribute(elt, "iconURL", aAddon.iconURL);
    checkAttribute(elt, "description", aAddon.description)
    checkAttribute(elt, "homepageURL", aAddon.homepageURL);
    checkAttribute(elt, "sourceURL", aAddon.sourceURL);
    ok(elt.install, "Extension has install property");
  }
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
  ExtensionsView.delayedInit();

  window.addEventListener("ViewChanged", function() {
    window.removeEventListener("ViewChanged", arguments.callee, true);
     aCallback();
  }, true);
}

function close_manager(aCallback) {
  let prefsButton = document.getElementById("tool-preferences");
  prefsButton.click();

  ExtensionsView.clearSection();
  ExtensionsView.clearSection("local");
  ExtensionsView._list = null;
  ExtensionsView._restartCount = 0;
  BrowserUI.hidePanel();

  if (aCallback)
    aCallback();
}

function loadUrl(aURL, aCallback, aNewTab) {
  if (aNewTab)
    gCurrentTab = Browser.addTab(aURL, true);
  else
    Browser.loadURI(aURL);

  gCurrentTab.browser.messageManager.addMessageListener("pageshow", function(aMessage) {
    if (gCurrentTab.browser.currentURI.spec == aURL) {
      gCurrentTab.browser.messageManager.removeMessageListener(aMessage.name, arguments.callee);
      if (aCallback)
        setTimeout(aCallback, 0);
    }
  });
}

function checkInstallPopup(aName, aCallback) {
  testPrompt("Installing Add-on", aName, [ {label: "Install", click: true},
                                           {label: "Cancel", click: false}],
                                  aCallback);
}

function testPrompt(aTitle, aMessage, aButtons, aCallback) {
  function doTest() {
    let prompt = document.getElementById("prompt-confirm-dialog");
    ok(!!prompt, "Prompt shown");

    if (prompt) {
      let title = document.getElementById("prompt-confirm-title");
      let message = document.getElementById("prompt-confirm-message");
      is(aTitle, title.textContent, "Correct title shown");
      is(aMessage, message.textContent, "Correct message shown");

      let buttons = document.querySelectorAll("#prompt-confirm-buttons-box .prompt-button");
      let clickButton = null;
      is(buttons.length, aButtons.length, "Prompt has correct number of buttons");
      if (buttons.length == aButtons.length) {
        for (let i = 0; i < buttons.length; i++) {
          is(buttons[i].label, aButtons[i].label, "Button has correct label");
          if (aButtons[i].click)
            clickButton = buttons[i];
        }
      }
      if (clickButton)
        clickButton.click();
    }
    if (aCallback)
      aCallback();
  }

  if (!document.getElementById("prompt-confirm-dialog")) {
    window.addEventListener("DOMWillOpenModalDialog", function() {
      window.removeEventListener("DOMWillOpenModalDialog", arguments.callee, true);
      // without this timeout, this can cause the prompt service to fail
      setTimeout(doTest, 500);
    }, true);
  } else {
    doTest();
  }
}

// Installs an addon via the urlbar.
function installFromURLBar(aAddon) {
  return function() {
    AddonManager.addInstallListener({
      onInstallEnded: function (install) {
        AddonManager.removeInstallListener(this);
        checkInstallNotification(!aAddon.bootstrapped, function() {
          open_manager(true, function() {
            isRestartShown(!aAddon.bootstrapped, false, function() {
              let elt = get_addon_element(aAddon.id);
              if (aAddon.bootstrapped) {
                checkAddonListing(aAddon, elt, "local");
                let button = document.getAnonymousElementByAttribute(elt, "anonid", "uninstall-button");
                ok(!!button, "Extension has uninstall button");

                let updateButton = document.getElementById("addons-update-all");
                is(updateButton.disabled, false, "Update button is enabled");

                ExtensionsView.uninstall(elt);

                elt = get_addon_element(aAddon.id);
                ok(!elt, "Addon element removed during uninstall");
                Browser.closeTab(gCurrentTab);
                close_manager(run_next_test);
              } else {
                ok(!elt, "Extension not in list");
                AddonManager.getAllInstalls(function(aInstalls) {
                  for(let i = 0; i < aInstalls.length; i++) {
                    aInstalls[i].cancel();
                  }
                  Browser.closeTab(gCurrentTab);
                  close_manager(run_next_test);
                });
              }
            });
          });
        });
      }
    });
    loadUrl(gTestURL, function() {
      loadUrl(aAddon.sourceURL, null, false);
      checkInstallAlert(true, function() {
        checkDownloadNotification(function() {
          checkInstallPopup(aAddon.name, function() { });
        });
      });
    }, true);
  };
}

// Installs an addon from the addons pref pane, and then
// updates it if requested. Checks to make sure
// restart notifications are shown at the right time
function installFromAddonsPage(aAddon, aDoUpdate) {
  return function() {
    open_manager(null, function() {
      let elt = get_addon_element(aAddon.id);
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

add_test(installFromURLBar(addons[0]));
add_test(installFromURLBar(addons[1]));
add_test(installFromAddonsPage(addons[0], true));
add_test(installFromAddonsPage(addons[1], false));

function installListener(aSettings) {
  this.onComplete = aSettings.onComplete;
  this.addon = aSettings.addon;
}

installListener.prototype = {
  onNewInstall : function(install) { },
  onDownloadStarted : function(install) { },
  onDownloadProgress : function(install) { },
  onDownloadEnded : function(install) { },
  onDownloadCancelled : function(install) { },
  onDownloadFailed : function(install) {
    if(this.addon.willFail)
      ok(false, "Install failed");
  },
  onInstallStarted : function(install) { },
  onInstallEnded : function(install, addon) {
    let self = this;
    isRestartShown(!this.addon.bootstrapped, false, function() {
      if(self.onComplete)
        self.onComplete();
    });
  },
  onInstallCancelled : function(install) { },
  onInstallFailed : function(install) {
    if(this.willFail)
      ok(false, "Install failed");
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
