// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var EXPORTED_SYMBOLS = ["RemoteWebProgressManager"];

const RemoteWebProgress = Components.Constructor(
  "@mozilla.org/dom/remote-web-progress;1",
  "nsIRemoteWebProgress",
  "init"
);

class RemoteWebProgressManager {
  constructor(aBrowser) {
    this._topLevelWebProgress = new RemoteWebProgress(
      this.QueryInterface(Ci.nsIWebProgress),
      /* aIsTopLevel = */ true
    );
    this._progressListeners = [];

    this.swapBrowser(aBrowser);
  }

  swapBrowser(aBrowser) {
    this._browser = aBrowser;
  }

  swapListeners(aOtherRemoteWebProgressManager) {
    let temp = aOtherRemoteWebProgressManager.progressListeners;
    aOtherRemoteWebProgressManager._progressListeners = this._progressListeners;
    this._progressListeners = temp;
  }

  get progressListeners() {
    return this._progressListeners;
  }

  get topLevelWebProgress() {
    return this._topLevelWebProgress;
  }

  addProgressListener(aListener, aNotifyMask) {
    let listener = aListener.QueryInterface(Ci.nsIWebProgressListener);
    this._progressListeners.push({
      listener,
      mask: aNotifyMask || Ci.nsIWebProgress.NOTIFY_ALL,
    });
  }

  removeProgressListener(aListener) {
    this._progressListeners = this._progressListeners.filter(
      l => l.listener != aListener
    );
  }

  setCurrentURI(aURI) {
    // This function is simpler than nsDocShell::SetCurrentURI since
    // it doesn't have to deal with child docshells.
    let remoteWebNav = this._browser._remoteWebNavigationImpl;
    remoteWebNav._currentURI = aURI;

    let webProgress = this.topLevelWebProgress;
    for (let { listener, mask } of this._progressListeners) {
      if (mask & Ci.nsIWebProgress.NOTIFY_LOCATION) {
        listener.onLocationChange(webProgress, null, aURI);
      }
    }
  }

  _callProgressListeners(type, methodName, ...args) {
    for (let { listener, mask } of this._progressListeners) {
      if (mask & type && listener[methodName]) {
        try {
          listener[methodName].apply(listener, args);
        } catch (ex) {
          Cu.reportError(
            "RemoteWebProgress failed to call " + methodName + ": " + ex + "\n"
          );
        }
      }
    }
  }

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_STATE_ALL,
      "onStateChange",
      aWebProgress,
      aRequest,
      aStateFlags,
      aStatus
    );
  }

  onProgressChange(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_PROGRESS,
      "onProgressChange",
      aWebProgress,
      aRequest,
      aCurSelfProgress,
      aMaxSelfProgress,
      aCurTotalProgress,
      aMaxTotalProgress
    );
  }

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_LOCATION,
      "onLocationChange",
      aWebProgress,
      aRequest,
      aLocation,
      aFlags
    );
  }

  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_STATUS,
      "onStatusChange",
      aWebProgress,
      aRequest,
      aStatus,
      aMessage
    );
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_SECURITY,
      "onSecurityChange",
      aWebProgress,
      aRequest,
      aState
    );
  }

  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
    this._callProgressListeners(
      Ci.nsIWebProgress.NOTIFY_CONTENT_BLOCKING,
      "onContentBlockingEvent",
      aWebProgress,
      aRequest,
      aEvent
    );
  }
}

RemoteWebProgressManager.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIWebProgress",
  "nsIWebProgressListener",
]);
