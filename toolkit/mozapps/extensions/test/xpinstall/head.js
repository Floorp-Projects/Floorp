const RELATIVE_DIR = "toolkit/mozapps/extensions/test/xpinstall/";

const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/browser/" + RELATIVE_DIR;
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PROMPT_URL = "chrome://global/content/commonDialog.xul";
const ADDONS_URL = "chrome://mozapps/content/extensions/extensions.xul";
const PREF_LOGGING_ENABLED = "extensions.logging.enabled";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const CHROME_NAME = "mochikit";

function getChromeRoot(path) {
  if (path === undefined) {
    return "chrome://" + CHROME_NAME + "/content/browser/" + RELATIVE_DIR
  }
  return getRootDirectory(path);
}

function extractChromeRoot(path) {
  var path = getChromeRoot(path);
  var jar = getJar(path);
  if (jar) {
    var tmpdir = extractJarToTmp(jar);
    return "file://" + tmpdir.path + "/";
  }
  return path;
}

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

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
  runningInstalls: null,

  // Setup and tear down functions
  setup: function() {
    waitForExplicitFinish();
    Services.prefs.setBoolPref(PREF_LOGGING_ENABLED, true);
    Services.obs.addObserver(this, "addon-install-started", false);
    Services.obs.addObserver(this, "addon-install-blocked", false);
    Services.obs.addObserver(this, "addon-install-failed", false);
    Services.obs.addObserver(this, "addon-install-complete", false);
    Services.wm.addListener(this);

    AddonManager.addInstallListener(this);
    this.installCount = 0;
    this.pendingCount = 0;
    this.runningInstalls = [];

    var self = this;
    registerCleanupFunction(function() {
      Services.prefs.clearUserPref(PREF_LOGGING_ENABLED);
      Services.obs.removeObserver(self, "addon-install-started");
      Services.obs.removeObserver(self, "addon-install-blocked");
      Services.obs.removeObserver(self, "addon-install-failed");
      Services.obs.removeObserver(self, "addon-install-complete");
      Services.wm.removeListener(self);

      AddonManager.removeInstallListener(self);
    });
  },

  finish: function() {
    AddonManager.getAllInstalls(function(installs) {
      is(installs.length, 0, "Should be no active installs at the end of the test");
      finish();
    });
  },

  endTest: function() {
    // Defer the final notification to allow things like the InstallTrigger
    // callback to complete
    var self = this;
    executeSoon(function() {
      let callback = self.installsCompletedCallback;
      let count = self.installCount;

      is(self.runningInstalls.length, 0, "Should be no running installs left");
      self.runningInstalls.forEach(function(aInstall) {
        info("Install for " + aInstall.sourceURI + " is in state " + aInstall.state);
      });

      self.installBlockedCallback = null;
      self.authenticationCallback = null;
      self.installConfirmCallback = null;
      self.downloadStartedCallback = null;
      self.downloadProgressCallback = null;
      self.downloadCancelledCallback = null;
      self.downloadFailedCallback = null;
      self.downloadEndedCallback = null;
      self.installStartedCallback = null;
      self.installFailedCallback = null;
      self.installEndedCallback = null;
      self.installsCompletedCallback = null;
      self.runningInstalls = null;

      callback(count);
    });
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
        var promptType = window.args.promptType;
        switch (promptType) {
          case "alert":
          case "alertCheck":
          case "confirmCheck":
          case "confirm":
          case "confirmEx":
                window.document.documentElement.acceptDialog();
                break;
          case "promptUserAndPass":
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
                break;
          default:
                ok(false, "prompt type " + promptType + " not handled in test.");
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
      installInfo.installs.forEach(function(install) {
        install.cancel();
      });
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
      domwindow.removeEventListener("load", arguments.callee, false);
      self.windowLoad(domwindow);
    }, false);
  },

  onCloseWindow: function(window) {
  },

  // Addon Install Listener

  onNewInstall: function(install) {
    this.runningInstalls.push(install);
  },

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
    isnot(this.runningInstalls.indexOf(install), -1,
          "Should only see cancelations for started installs");
    this.runningInstalls.splice(this.runningInstalls.indexOf(install), 1);

    if (this.downloadCancelledCallback)
      this.downloadCancelledCallback(install);
    this.checkTestEnded();
  },

  onDownloadFailed: function(install) {
    if (this.downloadFailedCallback)
      this.downloadFailedCallback(install);
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

  onInstallFailed: function(install) {
    if (this.installFailedCallback)
      this.installFailedCallback(install);
    this.checkTestEnded();
  },

  checkTestEnded: function() {
    if (--this.pendingCount == 0)
      this.endTest();
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    var installInfo = subject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    switch (topic) {
    case "addon-install-started":
      is(this.runningInstalls.length, installInfo.installs.length,
         "Should have seen the expected number of installs started");
      break;
    case "addon-install-blocked":
      this.installBlocked(installInfo);
      break;
    case "addon-install-failed":
      installInfo.installs.forEach(function(aInstall) {
        isnot(this.runningInstalls.indexOf(aInstall), -1,
              "Should only see failures for started installs");

        ok(aInstall.error != 0 || aInstall.addon.appDisabled,
           "Failed installs should have an error or be appDisabled");

        this.runningInstalls.splice(this.runningInstalls.indexOf(aInstall), 1);
      }, this);
      break;
    case "addon-install-complete":
      installInfo.installs.forEach(function(aInstall) {
        isnot(this.runningInstalls.indexOf(aInstall), -1,
              "Should only see completed events for started installs");

        is(aInstall.error, 0, "Completed installs should have no error");
        ok(!aInstall.appDisabled, "Completed installs should not be appDisabled");

        // Complete installs are either in the INSTALLED or CANCELLED state
        // since the test may cancel installs the moment they complete.
        ok(aInstall.state == AddonManager.STATE_INSTALLED ||
           aInstall.state == AddonManager.STATE_CANCELLED,
           "Completed installs should be in the right state");

        this.runningInstalls.splice(this.runningInstalls.indexOf(aInstall), 1);
      }, this);
      break;
    }
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsIWindowMediatorListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}
