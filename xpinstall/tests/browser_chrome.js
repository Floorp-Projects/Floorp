// Load in the test harness
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(window.location.href);
scriptLoader.loadSubScript(rootDir + "harness.js", this);

// ----------------------------------------------------------------------------
// Tests that starting a download from chrome works and bypasses the whitelist
function test() {
  waitForExplicitFinish();
  var xpimgr = Components.classes["@mozilla.org/xpinstall/install-manager;1"]
                         .createInstance(Components.interfaces.nsIXPInstallManager);
  xpimgr.initManagerFromChrome([ TESTROOT + "unsigned.xpi" ],
                               1, listener);
}

function finish_test() {
  var em = Components.classes["@mozilla.org/extensions/manager;1"]
                     .getService(Components.interfaces.nsIExtensionManager);
  em.cancelInstallItem("unsigned-xpi@tests.mozilla.org");

  finish();
}

var listener = {
  onStateChange: function(index, state, value) {
    is(index, 0, "There is only one download");
    if (state == Components.interfaces.nsIXPIProgressDialog.INSTALL_DONE)
      is(value, 0, "Install should have succeeded");
    else if (state == Components.interfaces.nsIXPIProgressDialog.DIALOG_CLOSE)
      finish_test();
  },

  onProgress: function(index, value, maxValue) {
    is(index, 0, "There is only one download");
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIXPIProgressDialog) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
// ----------------------------------------------------------------------------
