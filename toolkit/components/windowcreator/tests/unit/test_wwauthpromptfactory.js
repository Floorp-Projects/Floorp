var authPromptRequestReceived;

const tPFCID = Components.ID("{749e62f4-60ae-4569-a8a2-de78b649660f}");
const tPFContract = "@mozilla.org/passwordmanager/authpromptfactory;1";

/*
 * TestPromptFactory
 *
 * Implements nsIPromptFactory
 */
var TestPromptFactory = {
  QueryInterface: ChromeUtils.generateQI(["nsIFactory", "nsIPromptFactory"]),

  createInstance: function tPF_ci(outer, iid) {
    if (outer) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(iid);
  },

  lockFactory: function tPF_lockf(lock) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  getPrompt: function tPF_getPrompt(aWindow, aIID) {
    if (aIID.equals(Ci.nsIAuthPrompt) || aIID.equals(Ci.nsIAuthPrompt2)) {
      authPromptRequestReceived = true;
      return {};
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },
}; // end of TestPromptFactory implementation

/*
 * The tests
 */
function run_test() {
  Components.manager.nsIComponentRegistrar.registerFactory(
    tPFCID,
    "TestPromptFactory",
    tPFContract,
    TestPromptFactory
  );

  // Make sure that getting both nsIAuthPrompt and nsIAuthPrompt2 works
  // (these should work independently of whether the application has
  // nsIPromptService)
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].getService();

  authPromptRequestReceived = false;

  Assert.notEqual(ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt), null);

  Assert.ok(authPromptRequestReceived);

  authPromptRequestReceived = false;

  Assert.notEqual(ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt2), null);

  Assert.ok(authPromptRequestReceived);
}
