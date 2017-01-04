/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Sample for browser_AddonWatcher.js

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const TOPIC_BURNCPU = "test-addonwatcher-burn-some-cpu";
const TOPIC_BURNCPOW = "test-addonwatcher-burn-some-cpow";
const TOPIC_BURNCONTENTCPU = "test-addonwatcher-burn-some-content-cpu";
const TOPIC_READY = "test-addonwatcher-ready";

const MESSAGE_BURNCPOW = "test-addonwatcher-cpow:init";
const URL_FRAMESCRIPT = "chrome://addonwatcher-test/content/framescript.js";

Cu.import("resource://gre/modules/Services.jsm", this);
const { setTimeout } = Cu.import("resource://gre/modules/Timer.jsm", {});
Cu.import("resource://gre/modules/Task.jsm", this);

let globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
  .getService(Ci.nsIMessageListenerManager);

/**
 * Spend some time using CPU.
 */
function burnCPU() {
  let ignored = [];
  let start = Date.now();
  let i = 0;
  while (Date.now() - start < 1000) {
    ignored[i++ % 2] = i;
  }
}

/**
 * Spend some time in CPOW.
 */
function burnCPOW() {
  gBurnCPOW();
}
let gBurnCPOW = null;

function burnContentCPU() {
  setTimeout(() => {
 try {
    gBurnContentCPU()
  } catch (ex) {
    dump(`test-addon error: ${ex}\n`);
  }
}, 0);
}
let gBurnContentCPU = null;

let gTab = null;
let gTabBrowser = null;

function startup() {
  Services.obs.addObserver(burnCPU, TOPIC_BURNCPU, false);
  Services.obs.addObserver(burnCPOW, TOPIC_BURNCPOW, false);
  Services.obs.addObserver(burnContentCPU, TOPIC_BURNCONTENTCPU, false);

  let windows = Services.wm.getEnumerator("navigator:browser");
  let win = windows.getNext();
  gTabBrowser = win.gBrowser;
  gTab = gTabBrowser.addTab("about:robots");
  gBurnContentCPU = function() {
    gTab.linkedBrowser.messageManager.sendAsyncMessage("test-addonwatcher-burn-some-content-cpu", {});
  }

  gTab.linkedBrowser.messageManager.loadFrameScript(URL_FRAMESCRIPT, false);
  globalMM.loadFrameScript(URL_FRAMESCRIPT, false);

  if (Services.appinfo.browserTabsRemoteAutostart) {
    // This profile has e10s enabled, which means we'll want to
    // test CPOW traffic.
    globalMM.addMessageListener("test-addonwatcher-cpow:init", function waitForCPOW(msg) {
      if (Components.utils.isCrossProcessWrapper(msg.objects.burnCPOW)) {
        gBurnCPOW = msg.objects.burnCPOW;
        globalMM.removeMessageListener("test-addonwatcher-cpow:init", waitForCPOW);
        Services.obs.notifyObservers(null, TOPIC_READY, null);
      } else {
        Cu.reportError("test-addonwatcher-cpow:init didn't give us a CPOW! Expect timeouts.");
      }
    });
  } else {
    // e10s is not enabled, so a CPOW is not necessary - we can report ready
    // right away.
    Services.obs.notifyObservers(null, TOPIC_READY, null);
  }
}

function shutdown() {
  Services.obs.removeObserver(burnCPU, TOPIC_BURNCPU);
  Services.obs.removeObserver(burnCPOW, TOPIC_BURNCPOW);
  Services.obs.removeObserver(burnContentCPU, TOPIC_BURNCONTENTCPU);
  gTabBrowser.removeTab(gTab);
}

function install() {
  // Nothing to do
}

function uninstall() {
  // Nothing to do
}
