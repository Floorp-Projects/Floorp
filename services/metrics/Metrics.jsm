/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

#ifndef MERGED_COMPARTMENT

this.EXPORTED_SYMBOLS = ["Metrics"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

#endif

// We concatenate the JSMs together to eliminate compartment overhead.
// This is a giant hack until compartment overhead is no longer an
// issue.
#define MERGED_COMPARTMENT

#include providermanager.jsm
;
#include dataprovider.jsm
;
#include storage.jsm
;

this.Metrics = {
  ProviderManager: ProviderManager,
  DailyValues: DailyValues,
  Measurement: Measurement,
  Provider: Provider,
  Storage: MetricsStorageBackend,
  dateToDays: dateToDays,
  daysToDate: daysToDate,
};

