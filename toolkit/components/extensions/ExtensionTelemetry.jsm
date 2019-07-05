/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionTelemetry", "getTrimmedString"];

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

// Map of the base histogram ids for the metrics recorded for the extensions.
const histograms = {
  extensionStartup: "WEBEXT_EXTENSION_STARTUP_MS",
  backgroundPageLoad: "WEBEXT_BACKGROUND_PAGE_LOAD_MS",
  browserActionPopupOpen: "WEBEXT_BROWSERACTION_POPUP_OPEN_MS",
  browserActionPreloadResult: "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT",
  contentScriptInjection: "WEBEXT_CONTENT_SCRIPT_INJECTION_MS",
  pageActionPopupOpen: "WEBEXT_PAGEACTION_POPUP_OPEN_MS",
  storageLocalGetJSON: "WEBEXT_STORAGE_LOCAL_GET_MS",
  storageLocalSetJSON: "WEBEXT_STORAGE_LOCAL_SET_MS",
  storageLocalGetIDB: "WEBEXT_STORAGE_LOCAL_IDB_GET_MS",
  storageLocalSetIDB: "WEBEXT_STORAGE_LOCAL_IDB_SET_MS",
  userScriptInjection: "WEBEXT_USER_SCRIPT_INJECTION_MS",
};

/**
 * Get a trimmed version of the given string if it is longer than 80 chars (used in telemetry
 * when a string may be longer than allowed).
 *
 * @param {string} str
 *        The original string content.
 *
 * @returns {string}
 *          The trimmed version of the string when longer than 80 chars, or the given string
 *          unmodified otherwise.
 */
function getTrimmedString(str) {
  if (str.length <= 80) {
    return str;
  }

  const length = str.length;

  // Trim the string to prevent a flood of warnings messages logged internally by recordEvent,
  // the trimmed version is going to be composed by the first 40 chars and the last 37 and 3 dots
  // that joins the two parts, to visually indicate that the string has been trimmed.
  return `${str.slice(0, 40)}...${str.slice(length - 37, length)}`;
}

/**
 * This is a internal helper object which contains a collection of helpers used to make it easier
 * to collect extension telemetry (in both the general histogram and in the one keyed by addon id).
 *
 * This helper object is not exported from ExtensionUtils, it is used by the ExtensionTelemetry
 * Proxy which is exported and used by the callers to record telemetry data for one of the
 * supported metrics.
 */
class ExtensionTelemetryMetric {
  constructor(metric) {
    this.metric = metric;
  }

  // Stopwatch methods.
  stopwatchStart(extension, obj = extension) {
    this._wrappedStopwatchMethod("start", this.metric, extension, obj);
  }

  stopwatchFinish(extension, obj = extension) {
    this._wrappedStopwatchMethod("finish", this.metric, extension, obj);
  }

  stopwatchCancel(extension, obj = extension) {
    this._wrappedStopwatchMethod("cancel", this.metric, extension, obj);
  }

  // Histogram counters methods.
  histogramAdd(opts) {
    this._histogramAdd(this.metric, opts);
  }

  /**
   * Wraps a call to a TelemetryStopwatch method for a given metric and extension.
   *
   * @param {string} method
   *        The stopwatch method to call ("start", "finish" or "cancel").
   * @param {string} metric
   *        The stopwatch metric to record (used to retrieve the base histogram id from the _histogram object).
   * @param {Extension | BrowserExtensionContent} extension
   *        The extension to record the telemetry for.
   * @param {any | undefined} [obj = extension]
   *        An optional telemetry stopwatch object (which defaults to the extension parameter when missing).
   */
  _wrappedStopwatchMethod(method, metric, extension, obj = extension) {
    if (!extension) {
      throw new Error(`Mandatory extension parameter is undefined`);
    }

    const baseId = histograms[metric];
    if (!baseId) {
      throw new Error(`Unknown metric ${metric}`);
    }

    // Record metric in the general histogram.
    TelemetryStopwatch[method](baseId, obj);

    // Record metric in the histogram keyed by addon id.
    let extensionId = getTrimmedString(extension.id);
    TelemetryStopwatch[`${method}Keyed`](
      `${baseId}_BY_ADDONID`,
      extensionId,
      obj
    );
  }

  /**
   * Record a telemetry category and/or value for a given metric.
   *
   * @param {string} metric
   *        The metric to record (used to retrieve the base histogram id from the _histogram object).
   * @param {Object}                              options
   * @param {Extension | BrowserExtensionContent} options.extension
   *        The extension to record the telemetry for.
   * @param {string | undefined}                  [options.category]
   *        An optional histogram category.
   * @param {number | undefined}                  [options.value]
   *        An optional value to record.
   */
  _histogramAdd(metric, { category, extension, value }) {
    if (!extension) {
      throw new Error(`Mandatory extension parameter is undefined`);
    }

    const baseId = histograms[metric];
    if (!baseId) {
      throw new Error(`Unknown metric ${metric}`);
    }

    const histogram = Services.telemetry.getHistogramById(baseId);
    if (typeof category === "string") {
      histogram.add(category, value);
    } else {
      histogram.add(value);
    }

    const keyedHistogram = Services.telemetry.getKeyedHistogramById(
      `${baseId}_BY_ADDONID`
    );
    const extensionId = getTrimmedString(extension.id);

    if (typeof category === "string") {
      keyedHistogram.add(extensionId, category, value);
    } else {
      keyedHistogram.add(extensionId, value);
    }
  }
}

// Cache of the ExtensionTelemetryMetric instances that has been lazily created by the
// Extension Telemetry Proxy.
const metricsCache = new Map();

/**
 * This proxy object provides the telemetry helpers for the currently supported metrics (the ones listed in
 * ExtensionTelemetryHelpers._histograms), the telemetry helpers for a particular metric are lazily created
 * when the related property is being accessed on this object for the first time, e.g.:
 *
 *      ExtensionTelemetry.extensionStartup.stopwatchStart(extension);
 *      ExtensionTelemetry.browserActionPreloadResult.histogramAdd({category: "Shown", extension});
 */
var ExtensionTelemetry = new Proxy(metricsCache, {
  get(target, prop, receiver) {
    if (!(prop in histograms)) {
      throw new Error(`Unknown metric ${prop}`);
    }

    // Lazily create and cache the metric result object.
    if (!target.has(prop)) {
      target.set(prop, new ExtensionTelemetryMetric(prop));
    }

    return target.get(prop);
  },
});
