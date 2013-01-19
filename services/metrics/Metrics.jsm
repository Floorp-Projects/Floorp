/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Metrics"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/services/metrics/collector.jsm");
Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");
Cu.import("resource://gre/modules/services/metrics/storage.jsm");


this.Metrics = {
  Collector: Collector,
  DailyValues: DailyValues,
  Measurement: Measurement,
  Provider: Provider,
  Storage: MetricsStorageBackend,
  dateToDays: dateToDays,
  daysToDate: daysToDate,
};

