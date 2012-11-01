/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "PerfMeasurement" ];

/*
 * This is the js module for jsperf. Import it like so:
 *   Components.utils.import("resource://gre/modules/PerfMeasurement.jsm");
 *
 * This will create a 'PerfMeasurement' class.  Instances of this class can
 * be used to benchmark browser operations.
 *
 * For documentation on the API, see js/src/perf/jsperf.h.
 *
 */

Components.classes["@mozilla.org/jsperf;1"].createInstance()();
