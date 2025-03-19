/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRStartPageParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "NRStartPage:GetCurrentTopSites": {
        const { AboutNewTab } = ChromeUtils.importESModule(
          "resource:///modules/AboutNewTab.sys.mjs",
        );

        this.sendAsyncMessage(
          "NRStartPage:GetCurrentTopSites",
          JSON.stringify({
            topsites: AboutNewTab.getTopSites(),
          }),
        );
        break;
      }

      case "NRStartPage:GetCurrentBrowsingData": {
        this.sendAsyncMessage(
          "NRStartPage:GetCurrentBrowsingData",
          JSON.stringify({
            memoryUsage: Cc["@mozilla.org/memory-reporter-manager;1"]
              .getService(Ci.nsIMemoryReporterManager).resident,
            totalTabs: this.getTotalTabsCount(),
            totalBlockedTrackerCount: await this.getTotalBlockedTrackerCount(),
          }),
        );
        break;
      }
    }
  }

  getTotalTabsCount() {
    let totalTabs = 0;
    const enumerator = Services.wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      const win = enumerator.getNext() as Window;
      totalTabs += win.gBrowser.tabs.length;
    }
    return totalTabs;
  }

  async getTotalBlockedTrackerCount() {
    const tp = Cc["@mozilla.org/trackingprotection;1"]
      .getService(Ci.nsITrackingProtectionService);

    const count = tp.getTrackingContentBlockedCount();
    return count;
  }
}
