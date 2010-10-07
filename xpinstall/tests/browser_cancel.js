// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests that cancelling an in progress download works.
var gManager = null;

function test() {
  waitForExplicitFinish();
  gManager = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                       .createInstance(Components.interfaces.nsIXPInstallManager);
  gManager.initManagerFromChrome([ TESTROOT + "unsigned.xpi" ],
                                 1, listener);
}

function finish_test() {
  finish();
}

var listener = {
  onStateChange: function(index, state, value) {
    is(index, 0, "There is only one download");
    if (state == Components.interfaces.nsIXPIProgressDialog.INSTALL_DONE)
      is(value, -210, "Install should have been cancelled");
    else if (state == Components.interfaces.nsIXPIProgressDialog.DIALOG_CLOSE)
      finish_test();
  },

  onProgress: function(index, value, maxValue) {
    is(index, 0, "There is only one download");
    gManager.QueryInterface(Components.interfaces.nsIObserver);
    gManager.observe(null, "xpinstall-progress", "cancel");
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIXPIProgressDialog) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
// ----------------------------------------------------------------------------
