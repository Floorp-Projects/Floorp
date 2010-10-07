const TESTROOT = "http://example.com/browser/xpinstall/tests/";
const TESTROOT2 = "http://example.org/browser/xpinstall/tests/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PROMPT_URL = "chrome://global/content/commonDialog.xul";
const ADDONS_URL = "chrome://mozapps/content/extensions/extensions.xul";

var rootDir = getRootDirectory(gTestPath);
var path = rootDir.split('/');
var chromeName = path[0] + '//' + path[2];
var croot = chromeName + "/content/browser/xpinstall/tests/";
var jar = getJar(croot);
if (jar) {
  var tmpdir = extractJarToTmp(jar);
  croot = 'file://' + tmpdir.path + '/';
}
const CHROMEROOT = croot;

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
  // If set will be called when downloading of an item has ended.
  downloadEndedCallback: null,
  // If set will be called when installation by the extension manager of an xpi
  // item starts
  installStartedCallback: null,
  // If set will be called when each xpi item to be installed completes
  // installation.
  installEndedCallback: null,
  // If set will be called when all triggered items are installed or the install
  // is canceled.
  installsCompletedCallback: null,

  listenerIndex: null,

  // Setup and tear down functions
  setup: function() {
    waitForExplicitFinish();

    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "xpinstall-install-blocked", false);

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.addListener(this);

    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    this.listenerIndex = em.addInstallListener(this);
  },

  finish: function() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "xpinstall-install-blocked");

    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    wm.removeListener(this);
    var win = wm.getMostRecentWindow("Extension:Manager");
    if (win)
      win.close();

    var em = Components.classes["@mozilla.org/extensions/manager;1"]
                       .getService(Components.interfaces.nsIExtensionManager);
    em.removeInstallListenerAt(this.listenerIndex);
    finish();
  },

  endTest: function() {
    // Defer the final notification to allow things like the InstallTrigger
    // callback to complete
    executeSoon(this.installsCompletedCallback);

    this.installBlockedCallback = null;
    this.authenticationCallback = null;
    this.installConfirmCallback = null;
    this.downloadStartedCallback = null;
    this.downloadProgressCallback = null;
    this.downloadEndedCallback = null;
    this.installStartedCallback = null;
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
        case 0: if (window.opener.document.location.href == ADDONS_URL) {
                  // A prompt opened by the add-ons manager is liable to be an
                  // xpinstall error, just close it, we'll see the error in
                  // onInstallEnded anyway.
                  window.document.documentElement.acceptDialog();
                }
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
      var mgr = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                          .createInstance(Components.interfaces.nsIXPInstallManager);
      mgr.initManagerWithInstallInfo(installInfo);
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

  // nsIAddonInstallListener

  onDownloadStarted: function(addon) {
    if (this.downloadStartedCallback)
      this.downloadStartedCallback(addon);
  },

  onDownloadProgress: function(addon, value, maxValue) {
    if (this.downloadProgressCallback)
      this.downloadProgressCallback(addon, value, maxValue);
  },

  onDownloadEnded: function(addon) {
    if (this.downloadEndedCallback)
      this.downloadEndedCallback(addon);
  },

  onInstallStarted: function(addon) {
    if (this.installStartedCallback)
      this.installStartedCallback(addon);
  },

  onCompatibilityCheckStarted: function(addon) {
  },

  onCompatibilityCheckEnded: function(addon, status) {
  },

  onInstallEnded: function(addon, status) {
    if (this.installEndedCallback)
      this.installEndedCallback(addon, status);
  },

  onInstallsCompleted: function() {
    this.endTest();
  },

  // nsIObserver

  observe: function(subject, topic, data) {
    var installInfo = subject.QueryInterface(Components.interfaces.nsIXPIInstallInfo);
    this.installBlocked(installInfo);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIObserver) ||
        iid.equals(Components.interfaces.nsIAddonInstallListener) ||
        iid.equals(Components.interfaces.nsIWindowMediatorListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}
