/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewProcessHangMonitor"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

class GeckoViewProcessHangMonitor extends GeckoViewModule {
  constructor(aModuleInfo) {
    super(aModuleInfo);

    /**
     * Collection of hang reports that haven't expired or been dismissed
     * by the user. These are nsIHangReports.
     */
    this._activeReports = new Set();

    /**
     * Collection of hang reports that have been suppressed for a short
     * period of time. Keys are nsIHangReports. Values are timeouts for
     * when the wait time expires.
     */
    this._pausedReports = new Map();

    /**
     * Simple index used for report identification
     */
    this._nextIndex = 0;

    /**
     * Map of report IDs to report objects.
     * Keys are numbers. Values are nsIHangReports.
     */
    this._reportIndex = new Map();

    /**
     * Map of report objects to report IDs.
     * Keys are nsIHangReports. Values are numbers.
     */
    this._reportLookupIndex = new Map();
  }

  onInit() {
    debug`onInit`;
    Services.obs.addObserver(this, "process-hang-report");
    Services.obs.addObserver(this, "clear-hang-report");
  }

  onDestroy() {
    debug`onDestroy`;
    Services.obs.removeObserver(this, "process-hang-report");
    Services.obs.removeObserver(this, "clear-hang-report");
  }

  onEnable() {
    debug`onEnable`;
    this.registerListener([
      "GeckoView:HangReportStop",
      "GeckoView:HangReportWait",
    ]);
  }

  onDisable() {
    debug`onDisable`;
    this.unregisterListener();
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    if (this._reportIndex.has(aData.hangId)) {
      const report = this._reportIndex.get(aData.hangId);
      switch (aEvent) {
        case "GeckoView:HangReportStop":
          this.stopHang(report);
          break;
        case "GeckoView:HangReportWait":
          this.pauseHang(report);
          break;
      }
    } else {
      debug`Report not found: reportIndex=${this._reportIndex}`;
    }
  }

  // nsIObserver event handler
  observe(aSubject, aTopic, aData) {
    debug`observe(aTopic=${aTopic})`;
    aSubject.QueryInterface(Ci.nsIHangReport);
    if (!aSubject.isReportForBrowserOrChildren(this.browser.frameLoader)) {
      return;
    }

    switch (aTopic) {
      case "process-hang-report": {
        this.reportHang(aSubject);
        break;
      }
      case "clear-hang-report": {
        this.clearHang(aSubject);
        break;
      }
    }
  }

  /**
   * This timeout is the wait period applied after a user selects "Wait" in
   * an existing notification.
   */
  get WAIT_EXPIRATION_TIME() {
    try {
      return Services.prefs.getIntPref("browser.hangNotification.waitPeriod");
    } catch (ex) {
      return 10000;
    }
  }

  /**
   * Terminate whatever is causing this report, be it an add-on or page script.
   * This is done without updating any report notifications.
   */
  stopHang(report) {
    report.terminateScript();
  }

  /**
   *
   */
  pauseHang(report) {
    this._activeReports.delete(report);

    // Create a new timeout with notify callback
    const timer = this.window.setTimeout(() => {
      for (const [stashedReport, otherTimer] of this._pausedReports) {
        if (otherTimer === timer) {
          this._pausedReports.delete(stashedReport);

          // We're still hung, so move the report back to the active
          // list.
          this._activeReports.add(report);
          break;
        }
      }
    }, this.WAIT_EXPIRATION_TIME);

    this._pausedReports.set(report, timer);
  }

  /**
   * construct an information bundle
   */
  notifyReport(report) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:HangReport",
      hangId: this._reportLookupIndex.get(report),
      scriptFileName: report.scriptFileName,
    });
  }

  /**
   * Handle a potentially new hang report.
   */
  reportHang(report) {
    // if we aren't enabled then default to stopping the script
    if (!this.enabled) {
      this.stopHang(report);
      return;
    }

    // if we have already notified, remind
    if (this._activeReports.has(report)) {
      this.notifyReport(report);
      return;
    }

    // If this hang was already reported and paused by the user then ignore it.
    if (this._pausedReports.has(report)) {
      return;
    }

    const index = this._nextIndex++;
    this._reportLookupIndex.set(report, index);
    this._reportIndex.set(index, report);
    this._activeReports.add(report);

    // Actually notify the new report
    this.notifyReport(report);
  }

  clearHang(report) {
    this._activeReports.delete(report);

    const timer = this._pausedReports.get(report);
    if (timer) {
      this.window.clearTimeout(timer);
    }
    this._pausedReports.delete(report);

    if (this._reportLookupIndex.has(report)) {
      const index = this._reportLookupIndex.get(report);
      this._reportIndex.delete(index);
    }
    this._reportLookupIndex.delete(report);
    report.userCanceled();
  }
}

const { debug, warn } = GeckoViewProcessHangMonitor.initLogging(
  "GeckoViewProcessHangMonitor"
);
