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
          tabs[i].zombify();
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
