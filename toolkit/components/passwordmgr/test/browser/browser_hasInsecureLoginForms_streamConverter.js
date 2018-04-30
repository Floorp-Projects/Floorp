/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/LoginManagerParent.jsm", this);

function registerConverter() {
  ChromeUtils.import("resource://gre/modules/Services.jsm", this);
  ChromeUtils.import("resource://gre/modules/NetUtil.jsm", this);

  /**
   * Converts the "test/content" MIME type, served by the test over HTTP, to an
   * HTML viewer page containing the "form_basic.html" code. The viewer is
   * served from a "resource:" URI while keeping the "resource:" principal.
   */
  function TestStreamConverter() {}

  TestStreamConverter.prototype = {
    classID: Components.ID("{5f01d6ef-c090-45a4-b3e5-940d64713eb7}"),
    contractID: "@mozilla.org/streamconv;1?from=test/content&to=*/*",
    QueryInterface: ChromeUtils.generateQI([
      Ci.nsIRequestObserver,
      Ci.nsIStreamListener,
      Ci.nsIStreamConverter,
    ]),

    // nsIStreamConverter
    convert() {},

    // nsIStreamConverter
    asyncConvertData(aFromType, aToType, aListener, aCtxt) {
      this.listener = aListener;
    },

    // nsIRequestObserver
    onStartRequest(aRequest, aContext) {
      let channel = NetUtil.newChannel({
        uri: "resource://testing-common/form_basic.html",
        loadUsingSystemPrincipal: true,
      });
      channel.originalURI = aRequest.QueryInterface(Ci.nsIChannel).URI;
      channel.loadGroup = aRequest.loadGroup;
      channel.owner = Services.scriptSecurityManager
                              .createCodebasePrincipal(channel.URI, {});
      // In this test, we pass the new channel to the listener but don't fire a
      // redirect notification, even if it would be required. This keeps the
      // test code simpler and doesn't impact the principal check we're testing.
      channel.asyncOpen2(this.listener);
    },

    // nsIRequestObserver
    onStopRequest() {},

    // nsIStreamListener
    onDataAvailable() {},
  };

  let factory = XPCOMUtils._getFactory(TestStreamConverter);
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(TestStreamConverter.prototype.classID, "",
                            TestStreamConverter.prototype.contractID, factory);
  this.cleanupFunction = function() {
    registrar.unregisterFactory(TestStreamConverter.prototype.classID, factory);
  };
}

/**
 * Waits for the given number of occurrences of InsecureLoginFormsStateChange
 * on the given browser element.
 */
function waitForInsecureLoginFormsStateChange(browser, count) {
  return BrowserTestUtils.waitForEvent(browser, "InsecureLoginFormsStateChange",
                                       false, () => --count == 0);
}

/**
 * Checks that hasInsecureLoginForms is false for a viewer served internally
 * using a "resource:" URI.
 */
add_task(async function test_streamConverter() {
  let originalBrowser = gBrowser.selectedTab.linkedBrowser;

  await ContentTask.spawn(originalBrowser, null, registerConverter);

  let tab = BrowserTestUtils.addTab(gBrowser, "http://example.com/browser/toolkit/components/" +
                                   "passwordmgr/test/browser/streamConverter_content.sjs",
                                   { sameProcessAsFrameLoader: originalBrowser.frameLoader });
  let browser = tab.linkedBrowser;
  await Promise.all([
    BrowserTestUtils.switchTab(gBrowser, tab),
    BrowserTestUtils.browserLoaded(browser),
    // One event is triggered by pageshow and one by DOMFormHasPassword.
    waitForInsecureLoginFormsStateChange(browser, 2),
  ]);

  Assert.ok(!LoginManagerParent.hasInsecureLoginForms(browser));

  BrowserTestUtils.removeTab(tab);

  await ContentTask.spawn(originalBrowser, null, async function() {
    this.cleanupFunction();
  });
});
