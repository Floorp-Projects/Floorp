/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetworkObserver:
    "resource://devtools/shared/network-observer/NetworkObserver.sys.mjs",
});

/**
 * The NetworkThrottler uses the dev tools NetworkObserver to provide api to throttle all network activity.
 * This can be used to fix network conditions in browsertime pageload tests.
 *
 */

// A minimal struct for onNetworkEvent handling
class NetworkEventRecord {
  addRequestPostData() {}
  addResponseStart() {}
  addSecurityInfo() {}
  addEventTimings() {}
  addResponseCache() {}
  addResponseContent() {}
  addServerTimings() {}
  addServiceWorkerTimings() {}
}

class NetworkThrottler {
  #devtoolsNetworkObserver;
  #throttling;

  constructor() {
    this.#throttling = false;
  }

  destroy() {
    this.stop();
  }

  start(throttleData) {
    if (this.#throttling) {
      console.error("NetworkThrottler already started");
      return;
    }

    this.#devtoolsNetworkObserver = new lazy.NetworkObserver({
      ignoreChannelFunction: this.#ignoreChannelFunction,
      onNetworkEvent: this.#onNetworkEvent,
    });

    this.#devtoolsNetworkObserver.setThrottleData(throttleData);

    this.#throttling = true;
  }

  stop() {
    if (!this.#throttling) {
      return;
    }

    this.#devtoolsNetworkObserver.destroy();
    this.#devtoolsNetworkObserver = null;

    this.#throttling = false;
  }

  #ignoreChannelFunction = channel => {
    // Ignore chrome-privileged or DevTools-initiated requests
    if (
      channel.loadInfo?.loadingDocument === null &&
      (channel.loadInfo.loadingPrincipal ===
        Services.scriptSecurityManager.getSystemPrincipal() ||
        channel.loadInfo.isInDevToolsContext)
    ) {
      return true;
    }
    return false;
  };

  #onNetworkEvent = (networkEvent, channel) => {
    return new NetworkEventRecord(networkEvent, channel, this);
  };
}
