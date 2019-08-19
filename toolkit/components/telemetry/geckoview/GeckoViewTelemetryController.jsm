/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging(
  "GeckoView.TelemetryController"
); // eslint-disable-line no-unused-vars

var EXPORTED_SYMBOLS = ["GeckoViewTelemetryController"];

// Persistent data loading topic - see TelemetryGeckoViewPersistence.cpp.
const LOAD_COMPLETE_TOPIC = "internal-telemetry-geckoview-load-complete";

const GeckoViewTelemetryController = {
  /**
   * Setup the Telemetry recording flags. This must be called
   * in all the processes that need to collect Telemetry.
   */
  setup() {
    TelemetryUtils.setTelemetryRecordingFlags();

    debug`setup -
           canRecordPrereleaseData ${
             Services.telemetry.canRecordPrereleaseData
           },
           canRecordReleaseData ${Services.telemetry.canRecordReleaseData}`;

    if (GeckoViewUtils.IS_PARENT_PROCESS) {
      // Prevent dispatching snapshots before persistent data has been loaded.
      this._loadComplete = new Promise(resolve => {
        Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
          if (aTopic !== LOAD_COMPLETE_TOPIC) {
            warn`Received unexpected topic ${aTopic}`;
            return;
          }
          debug`observed ${aTopic} - ready to handle telemetry requests`;
          // Loading data has completed, discard this observer.
          Services.obs.removeObserver(observer, LOAD_COMPLETE_TOPIC);
          resolve();
        }, LOAD_COMPLETE_TOPIC);
      });

      try {
        EventDispatcher.instance.registerListener(this, [
          "GeckoView:TelemetrySnapshots",
        ]);
      } catch (e) {
        warn`Failed registering GeckoView:TelemetrySnapshots listener: ${e}`;
      }
    }
  },

  /**
   * Handle GeckoView:TelemetrySnapshots requests.
   * Match with RuntimeTelemetry.getSnapshots.
   *
   * @param aEvent Name of the event to dispatch.
   * @param aData Optional object containing data for the event.
   * @param aCallback Optional callback implementing nsIAndroidEventCallback.
   */
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: aEvent=${aEvent}, aData=${aData}`;

    if (aEvent !== "GeckoView:TelemetrySnapshots") {
      warn`Received unexpected event ${aEvent}`;
      return;
    }

    // Handle this request when loading has completed.
    this._loadComplete.then(() =>
      this.retrieveSnapshots(aData.clear, aCallback)
    );
  },

  /**
   * Retrieve snapshots and forward them to the callback.
   *
   * @param aClear True if snapshot data should be cleared after retrieving.
   * @param aCallback Callback implementing nsIAndroidEventCallback.
   */
  retrieveSnapshots(aClear, aCallback) {
    debug`retrieveSnapshots`;

    const histograms = Services.telemetry.getSnapshotForHistograms(
      "main",
      /* clear */ false
    );
    const keyedHistograms = Services.telemetry.getSnapshotForKeyedHistograms(
      "main",
      /* clear */ false
    );
    const scalars = Services.telemetry.getSnapshotForScalars(
      "main",
      /* clear */ false
    );
    const keyedScalars = Services.telemetry.getSnapshotForKeyedScalars(
      "main",
      /* clear */ false
    );

    const snapshots = {
      histograms,
      keyedHistograms,
      scalars,
      keyedScalars,
    };

    if (
      !snapshots.histograms ||
      !snapshots.keyedHistograms ||
      !snapshots.scalars ||
      !snapshots.keyedScalars
    ) {
      aCallback.onError(`Failed retrieving snapshots!`);
      return;
    }

    let processSnapshots = {};
    for (let [name, snapshot] of Object.entries(snapshots)) {
      for (let [processName, processSnapshot] of Object.entries(snapshot)) {
        if (!(processName in processSnapshots)) {
          processSnapshots[processName] = {};
        }
        processSnapshots[processName][name] = processSnapshot;
      }
    }

    if (aClear) {
      Services.telemetry.clearProbes();
    }

    aCallback.onSuccess(processSnapshots);
  },
};
