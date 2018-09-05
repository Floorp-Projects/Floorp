/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const gRegistrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

let gLoadedInProcess2Promise = null;

function _createProcessChooser(tabParent, from, to, rejectPromise = false) {
  let processChooser = new ProcessChooser(tabParent, "example.com", "example.org", rejectPromise);
  processChooser.onComplete = () => {
    gRegistrar.unregisterFactory(processChooser.classID, processChooser);
  };
  gRegistrar.registerFactory(processChooser.classID, "",
                            "@mozilla.org/network/processChooser",
                            processChooser);
  registerCleanupFunction(function() {
    if (processChooser.onComplete) {
      processChooser.onComplete();
    }
  });
}

function ProcessChooser(tabParent, from, to, rejectPromise = false) {
  this.tabParent = tabParent;
  this.fromDomain = from;
  this.toDomain = to;
  this.rejectPromise = rejectPromise;
}

ProcessChooser.prototype = {
  // nsIRedirectProcessChooser
  getChannelRedirectTarget: function(aChannel, aParentChannel, aIdentifier) {
    // Don't report failure when returning NS_ERROR_NOT_AVAILABLE
    expectUncaughtException(true);

    // let tabParent = aParentChannel.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsITabParent);
    if (this.channel && this.channel != aChannel) {
      // Hack: this is just so we don't get redirected multiple times.
      info("same channel. give null");
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }

    if (aChannel.URI.host != this.toDomain) {
      info("wrong host for channel " + aChannel.URI.host);
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }

    let redirects = aChannel.loadInfo.redirectChain;
    if (redirects[redirects.length - 1].principal.URI.host != this.fromDomain) {
      info("didn't find redirect");
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }

    info("returning promise");
    this.channel = aChannel;
    let self = this;

    expectUncaughtException(false);
    if (this.onComplete) {
      this.onComplete();
      this.onComplete = null;
    }
    aIdentifier.value = 42;
    return new Promise((resolve, reject) => {
      if (self.rejectPromise) {
        info("rejecting");
        reject(Cr.NS_ERROR_NOT_AVAILABLE);
        return;
      }
      // Can asyncly create a tab, or can resolve with a tab that was
      // previously created.
      info("resolving");
      resolve(self.tabParent);
    });
  },

  // nsIFactory
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  lockFactory: function() {},

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([Ci.nsIRedirectProcessChooser, Ci.nsIFactory]),
  classID: Components.ID("{62561fa8-c091-4c9f-8897-b59ae18b0979}")
}

add_task(async function() {
  info("Check that a redirect in process A may be correctly handled in process B");

  const kRoot1 = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                     "https://example.com/");
  const kRoot2 = getRootDirectory(gTestPath).replace("chrome://mochitests/content/",
                                                     "https://example.org/");
  const kRoot3 = getRootDirectory(gTestPath);

  // This process will attempt to load the page that redirects to a different origin
  let tab1 = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: kRoot1 + "dummy.html", forceNewProcess: true });
  // This process will eventually receive the redirected channel.
  let tab2 = await BrowserTestUtils.openNewForegroundTab({ gBrowser, url: kRoot2 + "dummy.html", forceNewProcess: true });

  let browser1 = gBrowser.getBrowserForTab(tab1);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  // This is for testing purposes only.
  // This "process chooser" will direct the channel to be opened in the second
  // tab, and thus in the second parent.
  let processChooser = _createProcessChooser(browser2.frameLoader.tabParent, "example.com", "example.org");

  info("Loading redirected URL");
  // Open the URL in the first process. We expect it to wind up in the second
  // process.

  // Define the child listener in the new channel.
  await ContentTask.spawn(browser2, null, async function(arg) {
    function ChannelListener(childListener) { this.childListener = childListener; }
    ChannelListener.prototype = {
      onStartRequest: function(aRequest, aContext) {
        info("onStartRequest");
        let channel = aRequest.QueryInterface(Ci.nsIChannel);
        Assert.equal(channel.URI.spec, this.childListener.URI, "Make sure the channel has the proper URI");
        Assert.equal(channel.originalURI.spec, this.childListener.originalURI, "Make sure the originalURI is correct");
      },
      onStopRequest: function(aRequest, aContext, aStatusCode) {
        info("onStopRequest");
        Assert.equal(aStatusCode, Cr.NS_OK, "Check the status code");
        Assert.equal(this.gotData, true, "Check that the channel received data");
        if (this.childListener.onComplete) {
          this.childListener.onComplete();
        }
        this.childListener.resolve();
      },
      onDataAvailable: function(aRequest, aContext, aInputStream,
                                aOffset, aCount) {
        this.gotData = true;
        info("onDataAvailable");
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIStreamListener,
                                              Ci.nsIRequestObserver])
    };

    function ChildListener(uri, originalURI, resolve) { this.URI = uri; this.originalURI = originalURI; this.resolve = resolve;}
    ChildListener.prototype = {
      // nsIChildProcessChannelListener
      onChannelReady: function(aChildChannel, aIdentifier) {
        Assert.equal(aIdentifier, 42, "Check the status code");
        info("onChannelReady");
        aChildChannel.completeRedirectSetup(new ChannelListener(this), null);
      },
      // nsIFactory
      createInstance: function(aOuter, aIID) {
        if (aOuter) {
          throw Cr.NS_ERROR_NO_AGGREGATION;
        }
        return this.QueryInterface(aIID);
      },
      lockFactory: function() {},
      // nsISupports
      QueryInterface: ChromeUtils.generateQI([Ci.nsIChildProcessChannelListener, Ci.nsIFactory]),
      classID: Components.ID("{a6c142a9-eb38-4a09-a940-b71cdad479e1}")
    }

    content.window.ChildListener = ChildListener;
  });

  // This promise instantiates a ChildListener and is resolved when the redirected
  // channel is completed.
  let loadedInProcess2Promise = ContentTask.spawn(browser2, { URI: kRoot2 + "dummy.html", originalURI: kRoot1 + "redirect.sjs?" + kRoot2 + "dummy.html"}, async function(arg) {
    // We register the listener in process no. 2
    return new Promise(resolve => {
      var childListener = new content.window.ChildListener(arg.URI, arg.originalURI, resolve);
      var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
      childListener.onComplete = () => {
        registrar.unregisterFactory(childListener.classID, childListener);
      }
      registrar.registerFactory(childListener.classID, "",
                              "@mozilla.org/network/childProcessChannelListener",
                              childListener);
    });
  });

  let browser1LoadHasStopped = BrowserTestUtils.browserStopped(browser1);

  await BrowserTestUtils.loadURI(browser1, kRoot1 + "redirect.sjs?" + kRoot2 + "dummy.html");

  // Check that the channel was delivered to process no. 2
  await loadedInProcess2Promise;
  info("channel has loaded in second process");
  // This is to check that the old channel was cancelled.
  await browser1LoadHasStopped;

  // check that a rejected promise also works.
  processChooser = _createProcessChooser(browser2.frameLoader.tabParent, "example.com", "example.org", true);
  let browser1LoadHasStoppedAgain = BrowserTestUtils.browserStopped(browser1);
  await BrowserTestUtils.loadURI(browser1, kRoot1 + "redirect.sjs?" + kRoot2 + "dummy.html");
  await browser1LoadHasStoppedAgain;
  info("this is done now");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  ok(true, "Got to the end of the test!");
});
