/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

class GeckoViewProgressContent extends GeckoViewContentModule {
  onInit() {
    debug `onInit`;

    ProgressTracker.onInit(this);

    const flags = Ci.nsIWebProgress.NOTIFY_PROGRESS |
                  Ci.nsIWebProgress.NOTIFY_STATE_NETWORK |
                  Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this.progressFilter, flags);
  }

  onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    debug `onProgressChange ${aCurSelf}/${aMaxSelf} ${aCurTotal}/${aMaxTotal}`;

    ProgressTracker.handleProgress(null, aCurTotal, aMaxTotal);
    ProgressTracker.updateProgress();
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    debug `onStateChange: isTopLevel=${aWebProgress.isTopLevel},
                          flags=${aStateFlags}, status=${aStatus}`;

    if (!aWebProgress || !aWebProgress.isTopLevel) {
      return;
    }

    const uri = aRequest.QueryInterface(Ci.nsIChannel).URI.displaySpec;
    debug `onStateChange: uri=${uri}`;

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      ProgressTracker.start(uri);
    } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
               !aWebProgress.isLoadingDocument) {
      ProgressTracker.stop();
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_REDIRECTING) {
      ProgressTracker.start(uri);
    }
  }

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug `onLocationChange: location=${aLocationURI.displaySpec},
                             flags=${aFlags}`;

    if (!aWebProgress || !aWebProgress.isTopLevel) {
      return;
    }

    if (aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      ProgressTracker.stop();
    } else {
      ProgressTracker.changeLocation(aLocationURI.displaySpec);
    }
  }
}

const ProgressTracker = {
  onInit: function(aModule) {
    this._module = aModule;
    this.clear();
  },

  start: function(aUri) {
    debug `ProgressTracker start ${aUri}`;

    if (this._tracking) {
      this.stop();
    }

    addEventListener("MozAfterPaint", this,
                     { capture: false, mozSystemGroup: true });
    addEventListener("DOMContentLoaded", this,
                     { capture: false, mozSystemGroup: true });
    addEventListener("pageshow", this,
                     { capture: false, mozSystemGroup: true });

    this._tracking = true;
    this.clear();
    let data = this._data;

    if (aUri === "about:blank") {
      data.uri = null;
      return;
    }

    data.uri = aUri;
    data.pageStart = true;
    this.updateProgress();
  },

  changeLocation: function(aUri) {
    debug `ProgressTracker changeLocation ${aUri}`;

    let data = this._data;
    data.locationChange = true;
    data.uri = aUri;
  },

  stop: function() {
    debug `ProgressTracker stop`;

    let data = this._data;
    data.pageStop = true;
    this.updateProgress();
    this._tracking = false;

    if (!data.parsed) {
      removeEventListener("DOMContentLoaded", this,
                          { capture: false, mozSystemGroup: true });
    }
    if (!data.firstPaint) {
      removeEventListener("MozAfterPaint", this,
                          { capture: false, mozSystemGroup: true });
    }
    if (!data.pageShow) {
      removeEventListener("pageshow", this,
                          { capture: false, mozSystemGroup: true });
    }
  },

  get messageManager() {
    return this._module.messageManager;
  },

  get eventDispatcher() {
    return this._module.eventDispatcher;
  },

  handleEvent: function(aEvent) {
    let data = this._data;

    const target = aEvent.originalTarget;
    const uri = target && target.location.href;

    if (!data.uri || data.uri !== uri) {
      return;
    }

    debug `ProgressTracker handleEvent: ${aEvent.type}`;

    let needsUpdate = false;

    switch (aEvent.type) {
      case "DOMContentLoaded":
        needsUpdate = needsUpdate || !data.parsed;
        data.parsed = true;
        removeEventListener("DOMContentLoaded", this,
                            { capture: false, mozSystemGroup: true });
        break;
      case "MozAfterPaint":
        needsUpdate = needsUpdate || !data.firstPaint;
        data.firstPaint = true;
        removeEventListener("MozAfterPaint", this,
                            { capture: false, mozSystemGroup: true });
        break;
      case "pageshow":
        needsUpdate = needsUpdate || !data.pageShow;
        data.pageShow = true;
        removeEventListener("pageshow", this,
                            { capture: false, mozSystemGroup: true });
        break;
    }

    if (needsUpdate) {
      this.updateProgress();
    }
  },

  clear: function() {
    this._data = {
      prev: 0,
      uri: null,
      locationChange: false,
      pageStart: false,
      pageStop: false,
      firstPaint: false,
      pageShow: false,
      parsed: false,
      totalReceived: 1,
      totalExpected: 1,
      channels: {},
    };
  },

  _debugData: function() {
    return {
      prev: this._data.prev,
      uri: this._data.uri,
      locationChange: this._data.locationChange,
      pageStart: this._data.pageStart,
      pageStop: this._data.pageStop,
      firstPaint: this._data.firstPaint,
      pageShow: this._data.pageShow,
      parsed: this._data.parsed,
      totalReceived: this._data.totalReceived,
      totalExpected: this._data.totalExpected,
    };
  },

  handleProgress: function(aChannelUri, aProgress, aMax) {
    debug `ProgressTracker handleProgress ${aChannelUri} ${aProgress}/${aMax}`;

    let data = this._data;

    if (!data.uri) {
      return;
    }

    aChannelUri = aChannelUri || data.uri;

    const now = content.performance.now();

    if (!data.channels[aChannelUri]) {
      data.channels[aChannelUri] = {
        received: aProgress,
        max: aMax,
        expected: (aMax > 0 ? aMax : aProgress * 2),
        lastUpdate: now,
      };
    } else {
      let channelProgress = data.channels[aChannelUri];
      channelProgress.received = Math.max(channelProgress.received, aProgress);
      channelProgress.expected = Math.max(channelProgress.expected, aMax);
      channelProgress.lastUpdate = now;
    }
  },

  updateProgress: function() {
    debug `ProgressTracker updateProgress`;

    let data = this._data;

    if (!this._tracking || !data.uri) {
      return;
    }

    let progress = 0;

    if (data.pageStart) {
      progress += 10;
    }
    if (data.firstPaint) {
      progress += 15;
    }
    if (data.parsed) {
      progress += 15;
    }
    if (data.pageShow) {
      progress += 15;
    }
    if (data.locationChange) {
      progress += 10;
    }

    data.totalReceived = 1;
    data.totalExpected = 1;
    const channelOverdue = content.performance.now() - 300;

    for (let channel in data.channels) {
      if (data.channels[channel].max < 1 &&
          channelOverdue > data.channels[channel].lastUpdate) {
        data.channels[channel].expected = data.channels[channel].received;
      }
      data.totalReceived += data.channels[channel].received;
      data.totalExpected += data.channels[channel].expected;
    }

    const minExpected = 1024 * 1;
    const maxExpected = 1024 * 1024 * 0.5;

    if (data.pageStop ||
        (data.pageStart && data.firstPaint && data.parsed && data.pageShow &&
         data.totalReceived > minExpected &&
         data.totalReceived >= data.totalExpected)) {
      progress = 100;
    } else {
      const a = Math.min(1, (data.totalExpected / maxExpected)) * 30;
      progress += data.totalReceived / data.totalExpected * a;
    }

    debug `ProgressTracker onProgressChangeUpdate ${this._debugData()} ${data.totalReceived}/${data.totalExpected} progress=${progress}`;

    if (data.prev >= progress) {
      return;
    }

    this.eventDispatcher.sendRequest({
      type: "GeckoView:ProgressChanged",
      progress,
    });

    data.prev = progress;
  },
};


let {debug, warn} = GeckoViewProgressContent.initLogging("GeckoViewProgress");
let module = GeckoViewProgressContent.create(this);
