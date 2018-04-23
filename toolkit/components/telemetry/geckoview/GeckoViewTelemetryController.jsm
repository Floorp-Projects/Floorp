/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
});

GeckoViewUtils.initLogging("GeckoView.TelemetryController", this);

var EXPORTED_SYMBOLS = ["GeckoViewTelemetryController"];

/* global debug warn */

/**
 * Telemetry snapshot API adaptors used to retrieve one or more snapshots
 * for GeckoView:TelemetrySnapshots requests.
 * Match with RuntimeTelemetry.SNAPSHOT_* and nsITelemetry.idl.
 */
const TelemetrySnapshots = [
  {
    type: "histograms",
    flag: (1 << 0),
    get: (dataset, clear) => Services.telemetry.snapshotHistograms(
                               dataset, false, clear)
  },
  {
    type: "keyedHistograms",
    flag: (1 << 1),
    get: (dataset, clear) => Services.telemetry.snapshotKeyedHistograms(
                               dataset, false, clear)
  },
  {
    type: "scalars",
    flag: (1 << 2),
    get: (dataset, clear) => Services.telemetry.snapshotScalars(
                               dataset, clear)
  },
  {
    type: "keyedScalars",
    flag: (1 << 3),
    get: (dataset, clear) => Services.telemetry.snapshotKeyedScalars(
                               dataset, clear)
  },
];

const GeckoViewTelemetryController = {
  /**
   * Setup the Telemetry recording flags. This must be called
   * in all the processes that need to collect Telemetry.
   */
  setup() {
    debug `setup`;

    TelemetryUtils.setTelemetryRecordingFlags();

    debug `setup - canRecordPrereleaseData ${Services.telemetry.canRecordPrereleaseData
          }, canRecordReleaseData ${Services.telemetry.canRecordReleaseData}`;

    if (GeckoViewUtils.IS_PARENT_PROCESS) {
      try {
        EventDispatcher.instance.registerListener(this, [
          "GeckoView:TelemetrySnapshots",
        ]);
      } catch (e) {
        warn `Failed registering GeckoView:TelemetrySnapshots listener: ${e}`;
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
    debug `onEvent: aEvent=${aEvent}, aData=${aData}`;

    if (aEvent !== "GeckoView:TelemetrySnapshots") {
      warn `Received unexpected event ${aEvent}`;
      return;
    }

    const { clear, types, dataset } = aData;
    let snapshots = {};

    // Iterate over all snapshot types, retreive and assemble results.
    for (const tel of TelemetrySnapshots) {
      if ((tel.flag & types) == 0) {
        // This snapshot type has not been requested.
        continue;
      }
      const snapshot = tel.get(dataset, clear);
      if (!snapshot) {
        aCallback.onError(`Failed retrieving ${tel.type} snapshot!`);
        return;
      }
      snapshots[tel.type] = snapshot;
    }

    aCallback.onSuccess(snapshots);
  },
};
