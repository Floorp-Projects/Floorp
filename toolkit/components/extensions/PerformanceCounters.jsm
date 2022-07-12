/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module contains a global counter to store API call in the current process.
 */

/* exported Counters */
var EXPORTED_SYMBOLS = ["PerformanceCounters"];

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);

const { DefaultMap } = ExtensionUtils;

const lazy = {};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTimingEnabled",
  "extensions.webextensions.enablePerformanceCounters",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTimingMaxAge",
  "extensions.webextensions.performanceCountersMaxAge",
  1000
);

class CounterMap extends DefaultMap {
  defaultConstructor() {
    return new DefaultMap(() => ({ duration: 0, calls: 0 }));
  }

  flush() {
    let result = new CounterMap(undefined, this);
    this.clear();
    return result;
  }

  merge(other) {
    for (let [webextId, counters] of other) {
      for (let [api, counter] of counters) {
        let current = this.get(webextId).get(api);
        current.calls += counter.calls;
        current.duration += counter.duration;
      }
    }
  }
}

/**
 * Global Deferred used to send to the parent performance counters
 * when the counter is in a child.
 */
var _performanceCountersSender = null;

// Pre-definition of the global Counters instance.
var PerformanceCounters = null;

function _sendPerformanceCounters(childApiManagerId) {
  let counters = PerformanceCounters.flush();
  // No need to send empty counters.
  if (counters.size == 0) {
    return;
  }
  let options = { childId: childApiManagerId, counters: counters };
  Services.cpmm.sendAsyncMessage("Extension:SendPerformanceCounter", options);
}

class Counters {
  constructor() {
    this.data = new CounterMap();
  }

  /**
   * Returns true if performance counters are enabled.
   *
   * Indirection used so gTimingEnabled is not exposed direcly
   * in PerformanceCounters -- which would prevent tests to dynamically
   * change the preference value once PerformanceCounters.jsm is loaded.
   *
   * @returns {boolean}
   */
  get enabled() {
    return lazy.gTimingEnabled;
  }

  /**
   * Returns the counters max age
   *
   * Indirection used so gTimingMaxAge is not exposed direcly
   * in PerformanceCounters -- which would prevent tests to dynamically
   * change the preference value once PerformanceCounters.jsm is loaded.
   *
   * @returns {number}
   */
  get maxAge() {
    return lazy.gTimingMaxAge;
  }

  /**
   * Stores an execution time.
   *
   * @param {string} webExtensionId The web extension id.
   * @param {string} apiPath The API path.
   * @param {integer} duration How long the call took.
   * @param {childApiManagerId} childApiManagerId If executed from a child, its API manager id.
   */
  storeExecutionTime(webExtensionId, apiPath, duration, childApiManagerId) {
    let apiCounter = this.data.get(webExtensionId).get(apiPath);
    apiCounter.duration += duration;
    apiCounter.calls += 1;

    // Create the global deferred task if we're in a child and
    // it's the first time.
    if (childApiManagerId) {
      if (!_performanceCountersSender) {
        _performanceCountersSender = new DeferredTask(() => {
          _sendPerformanceCounters(childApiManagerId);
        }, this.maxAge);
      }
      _performanceCountersSender.arm();
    }
  }

  /**
   * Merges another CounterMap into this.data
   *
   * Can be used by the main process to merge data received
   * from the children.
   *
   * @param {CounterMap} data The map to merge.
   */
  merge(data) {
    this.data.merge(data);
  }

  /**
   * Returns the performance counters and purges them.
   *
   * @returns {CounterMap}
   */
  flush() {
    return this.data.flush();
  }

  /**
   * Returns the performance counters.
   *
   * @returns {CounterMap}
   */
  getData() {
    return this.data;
  }
}

PerformanceCounters = new Counters();
