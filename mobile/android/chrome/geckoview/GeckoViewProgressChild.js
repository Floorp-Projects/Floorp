/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {GeckoViewChildModule} = ChromeUtils.import("resource://gre/modules/GeckoViewChildModule.jsm");
var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {HistogramStopwatch} = ChromeUtils.import("resource://gre/modules/GeckoViewTelemetry.jsm");

const PAGE_LOAD_PROGRESS_PROBE =
  new HistogramStopwatch("GV_PAGE_LOAD_PROGRESS_MS", content);
const PAGE_LOAD_PROBE = new HistogramStopwatch("GV_PAGE_LOAD_MS", content);

class GeckoViewProgressChild extends GeckoViewChildModule {
  onInit() {
    debug `onInit`;

    ProgressTracker.onInit(this);

    const flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK |
                  Ci.nsIWebProgress.NOTIFY_LOCATION;
    this.progressFilter =
      Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    this.progressFilter.addProgressListener(this, flags);
    const webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this.progressFilter, flags);
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    debug `onStateChange: isTopLevel=${aWebProgress.isTopLevel},
                          flags=${aStateFlags}, status=${aStatus}`;

    if (!aWebProgress || !aWebProgress.isTopLevel) {
      return;
    }

    const uri = aRequest.QueryInterface(Ci.nsIChannel).URI.displaySpec;

    if (aRequest.URI.schemeIs("about")) {
      return;
    }

    debug `onStateChange: uri=${uri}`;

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      PAGE_LOAD_PROBE.start();
      ProgressTracker.start(uri);
    } else if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
               !aWebProgress.isLoadingDocument) {
      PAGE_LOAD_PROBE.finish();
      ProgressTracker.stop();
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_REDIRECTING) {
      PAGE_LOAD_PROBE.start();
      ProgressTracker.start(uri);
    }
  }

  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    if (!aWebProgress || !aWebProgress.isTopLevel ||
        !aLocationURI || aLocationURI.schemeIs("about")) {
      return;
    }

    debug `onLocationChange: location=${aLocationURI.displaySpec},
                             flags=${aFlags}`;

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
      PAGE_LOAD_PROGRESS_PROBE.cancel();
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

    PAGE_LOAD_PROGRESS_PROBE.start();

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
    };
  },

  updateProgress: function() {
    debug `ProgressTracker updateProgress`;

    let data = this._data;

    if (!this._tracking || !data.uri) {
      return;
    }

    let progress = 0;
    if (data.pageStop || data.pageShow) {
      progress = 100;
    } else if (data.firstPaint) {
      progress = 80;
    } else if (data.parsed) {
      progress = 55;
    } else if (data.locationChange) {
      progress = 30;
    } else if (data.pageStart) {
      progress = 15;
    }

    if (data.prev >= progress) {
      return;
    }

    debug `ProgressTracker updateProgress data=${this._debugData()}
           progress=${progress}`;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:ProgressChanged",
      progress,
    });

    data.prev = progress;

    if (progress >= 100) {
      PAGE_LOAD_PROGRESS_PROBE.finish();
    }
  },
};

const {debug, warn} = GeckoViewProgressChild.initLogging("GeckoViewProgress"); // eslint-disable-line no-unused-vars
const module = GeckoViewProgressChild.create(this);
