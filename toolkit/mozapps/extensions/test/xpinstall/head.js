const RELATIVE_DIR = "toolkit/mozapps/extensions/test/xpinstall/";

const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/browser/" + RELATIVE_DIR;
const CHROMEROOT = "chrome://mochikit/content/browser/" + RELATIVE_DIR;
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PROMPT_URL = "chrome://global/content/commonDialog.xul";
const ADDONS_URL = "chrome://mozapps/content/extensions/extensions.xul";

Components.utils.import("resource://gre/modules/AddonManager.jsm");

/**
 * This is a test harness designed to handle responding to UI during the process
 * of installing an XPI. A test can set callbacks to hear about specific parts
 * of the sequence.
 * Before use setup must be called and finish must be called afterwards.
 */
var Harness = {
  // If set then the install is expected to be blocked by the whitelist. The
  // callback should return true to continue with the install anyway.
  installBlockedCallback: null,
  // If set will be called in the event of authentication being needed to get
  // the xpi. Should return a 2 element array of username and password, or
  // null to not authenticate.
  authenticationCallback: null,
  // If set this will be called to allow checking the contents of the xpinstall
  // confirmation dialog. The callback should return true to continue the install.
  installConfirmCallback: null,
  // If set will be called when downloading of an item has begun.
  downloadStartedCallback: null,
  // If set will be called during the download of an item.
  downloadProgressCallback: null,
  // If set will be called when an xpi fails to download.
  downloadFailedCallback: null,
  // If set will be called when an xpi download is cancelled.
  downloadCancelledCallback: null,
  // If set will be called when downloading of an item has ended.
  downloadEndedCallback: null,
  // If set will be called when installation by the extension manager of an xpi
  // item starts
  installStartedCallback: null,
  // If set will be called when an xpi fails to install.
  installFailedCallback: null,
  // If set will be called when each xpi item to be installed completes
  // installation.
  installEndedCallback: null,
  // If set will be called when all triggered items are installed or the install
  // is canceled.
  installsCompletedCallback: null,

  pendingCount: null,
  installCount: null,

  // Setup and tear down functions
  setup: function() {
    waitForExplicitFinish();

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "addon-install-blocked", false);

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.addListener(this);

    AddonManager.addInstallListener(this);
    this.installCount = 0;
    this.pendingCount = 0;
  },

  finish: function() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "addon-install-blocked");

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.removeListener(this);
    var win = wm.getMostRecentWindow("Extension:Manager");
    if (win)
      win.close();

    AddonManager.removeInstallListener(this);
    finish();
  },

  endTest: function() {
    // Defer the final notification to allow things like the InstallTrigger
    // callback to complete
    let callback = this.installsCompletedCallback;
    let count = this.installCount;
    executeSoon(function() {
      callback(count);
    });

    this.installBlockedCallback = null;
    this.authenticationCallback = null;
    this.installConfirmCallback = null;
    this.downloadStartedCallback = null;
    this.downloadProgressCallback = null;
    this.downloadCancelledCallback = null;
    this.downloadFailedCallback = null;
    this.downloadEndedCallback = null;
    this.installStartedCallback = null;
    this.installFailedCallback = null;
    this.installEndedCallback = null;
    this.installsCompletedCallback = null;
  },

  // Window open handling
  windowLoad: function(window) {
    // Allow any other load handlers to execute
    var self = this;
    executeSoon(function() { self.windowReady(window); } );
  },

  windowReady: function(window) {
    if (window.document.location.href == XPINSTALL_URL) {
      if (this.installBlockedCallback)
        ok(false, "Should have been blocked by the whitelist");
      this.pendingCount = window.document.getElementById("itemList").childNodes.length;

      // If there is a confirm callback then its return status determines whether
      // to install the items or not. If not the test is over.
      if (this.installConfirmCallback && !this.installConfirmCallback(window)) {
        window.document.documentElement.cancelDialog();
        this.endTest();
      }
      else {
        // Initially the accept button is disabled on a countdown timer
        var button = window.document.documentElement.getButton("accept");
        button.disabled = false;
        window.document.documentElement.acceptDialog();
      }
    }
    else if (window.document.location.href == PROMPT_URL) {
      switch (window.gCommonDialogParam.GetInt(3)) {
        case 0: window.document.documentElement.acceptDialog();
                break;
        case 2: if (window.gCommonDialogParam.GetInt(4) != 1) {
                  // This is a login dialog, hopefully an authentication prompt
                  // for the xpi.
                  if (this.authenticationCallback) {
                    var auth = this.authenticationCallback();
                    if (auth && auth.length == 2) {
                      window.document.getElementById("loginTextbox").value = auth[0];
                      window.document.getElementById("password1Textbox").value = auth[1];
                      window.document.documentElement.acceptDialog();
                    }
                    else {
                      window.document.documentElement.cancelDialog();
                    }
                  }
                  else {
                    window.document.documentElement.cancelDialog();
                  }
                }
                break;
      }
    }
  },

  // Install blocked handling

  installBlocked: function(installInfo) {
    ok(!!this.installBlockedCallback, "Shouldn't have been blocked by the whitelist");
    if (this.installBlockedCallback && this.installBlockedCallback(installInfo)) {
      this.installBlockedCallback = null;
      installInfo.install();
    }
    else {
      this.endTest();
    }
  },

  // nsIWindowMediatorListener

  onWindowTitleChange: function(window, title) {
  },

  onOpenWindow: function(window) {
    var domwindow = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                          .getInterface(Components.interfaces.nsIDOMWindowInternal);
    var self = this;
    domwindow.addEventListener("load", function() {
      self.windowLoad(domwindow);
    }, false);
  },

  onCloseWindow: function(window) {
  },

  // Addon Install Listener

  onDownloadStarted: function(install) {
    this.pendingCount++;
    if (this.downloadStartedCallback)
      this.downloadStartedCallback(install);
  },

  onDownloadProgress: function(install) {
    if (this.downloadProgressCallback)
      this.downloadProgressCallback(install);
  },

  onDownloadEnded: function(install) {
    if (this.downloadEndedCallback)
      this.downloadEndedCallback(install);
  },

  onDownloadCancelled: function(install) {
    if (this.downloadCancelledCallback)
      this.downloadCancelledCallback(install);
    this.checkTestEnded();
  },

  onDownloadFailed: function(install, status) {
    if (this.downloadFailedCallback)
      this.downloadFailedCallback(install, status);
    this.checkTestEnded();
  },

  onInstallStarted: function(install) {
    if (this.installStartedCallback)
      this.installStartedCallback(install);
  },

  onInstallEnded: function(install, addon) {
    if (this.installEndedCallback)
      this.installEndedCallback(install, addon);
    this.installCount++;
    this.checkTestEnded();
  },

  onInstallFailed: function(install, status) {
    if (this.installFailedCallback)
      this.installFailedCallback(install, status);
    this.checkTestEnded();
  },

  checkTestEnded: function() {
    dump("checkTestPending " + this.pendingCount + "\n");
    if (--this.pendingCount == 0)
      this.endTest();
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    var installInfo = subject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    this.installBlocked(installInfo);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsIWindowMediatorListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}
