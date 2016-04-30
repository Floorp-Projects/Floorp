/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var MemoryObserver = {
  observe: function mo_observe(aSubject, aTopic, aData) {
    if (aTopic == "memory-pressure") {
      if (aData != "heap-minimize") {
        this.handleLowMemory();
      }
      // The JS engine would normally GC on this notification, but since we
      // disabled that in favor of this method (bug 669346), we should gc here.
      // See bug 784040 for when this code was ported from XUL to native Fennec.
      this.gc();
    } else if (aTopic == "Memory:Dump") {
      this.dumpMemoryStats(aData);
    }
  },

  handleLowMemory: function() {
    // do things to reduce memory usage here
    if (!Services.prefs.getBoolPref("browser.tabs.disableBackgroundZombification")) {
      let tabs = BrowserApp.tabs;
      let selected = BrowserApp.selectedTab;
      for (let i = 0; i < tabs.length; i++) {
        if (tabs[i] != selected && !tabs[i].playingAudio) {
          this.zombify(tabs[i]);
        }
      }
    }

    // Change some preferences temporarily for only this session
    let defaults = Services.prefs.getDefaultBranch(null);

    // Reduce the amount of decoded image data we keep around
    defaults.setIntPref("image.mem.max_decoded_image_kb", 0);

    // Stop using the bfcache
    if (!Services.prefs.getBoolPref("browser.sessionhistory.bfcacheIgnoreMemoryPressure")) {
      defaults.setIntPref("browser.sessionhistory.max_total_viewers", 0);
    }
  },

  zombify: function(tab) {
    let browser = tab.browser;
    let data = browser.__SS_data;
    let extra = browser.__SS_extdata;

    // Notify any interested parties (e.g. the session store)
    // that the original tab object is going to be destroyed
    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabPreZombify", true, false, window, null);
    browser.dispatchEvent(evt);

    // We need this data to correctly create and position the new browser
    // If this browser is already a zombie, fallback to the session data
    let currentURL = browser.__SS_restore ? data.entries[0].url : browser.currentURI.spec;
    let sibling = browser.nextSibling;
    let isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);

    tab.destroy();
    tab.create(currentURL, { sibling: sibling, zombifying: true, delayLoad: true, isPrivate: isPrivate });

    // Reattach session store data and flag this browser so it is restored on select
    browser = tab.browser;
    browser.__SS_data = data;
    browser.__SS_extdata = extra;
    browser.__SS_restore = true;
    browser.setAttribute("pending", "true");

    // Notify the session store to reattach its listeners to the new tab object
    evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabPostZombify", true, false, window, null);
    browser.dispatchEvent(evt);
  },

  gc: function() {
    window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).garbageCollect();
    Cu.forceGC();
  },

  dumpMemoryStats: function(aLabel) {
    let memDumper = Cc["@mozilla.org/memory-info-dumper;1"].getService(Ci.nsIMemoryInfoDumper);
    memDumper.dumpMemoryInfoToTempDir(aLabel, /* anonymize = */ false,
                                      /* minimize = */ false);
  },
};
