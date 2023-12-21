/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

const { DefaultWeakMap } = ExtensionUtils;

// Map of the base histogram ids for the metrics recorded for the extensions.
const HISTOGRAMS_IDS = {
  backgroundPageLoad: "WEBEXT_BACKGROUND_PAGE_LOAD_MS",
  browserActionPopupOpen: "WEBEXT_BROWSERACTION_POPUP_OPEN_MS",
  browserActionPreloadResult: "WEBEXT_BROWSERACTION_POPUP_PRELOAD_RESULT_COUNT",
  contentScriptInjection: "WEBEXT_CONTENT_SCRIPT_INJECTION_MS",
  eventPageRunningTime: "WEBEXT_EVENTPAGE_RUNNING_TIME_MS",
  eventPageIdleResult: "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT",
  extensionStartup: "WEBEXT_EXTENSION_STARTUP_MS",
  pageActionPopupOpen: "WEBEXT_PAGEACTION_POPUP_OPEN_MS",
  storageLocalGetJson: "WEBEXT_STORAGE_LOCAL_GET_MS",
  storageLocalSetJson: "WEBEXT_STORAGE_LOCAL_SET_MS",
  storageLocalGetIdb: "WEBEXT_STORAGE_LOCAL_IDB_GET_MS",
  storageLocalSetIdb: "WEBEXT_STORAGE_LOCAL_IDB_SET_MS",
};

const GLEAN_METRICS_TYPES = {
  backgroundPageLoad: "timing_distribution",
  browserActionPopupOpen: "timing_distribution",
  browserActionPreloadResult: "labeled_counter",
  contentScriptInjection: "timing_distribution",
  eventPageRunningTime: "custom_distribution",
  eventPageIdleResult: "labeled_counter",
  extensionStartup: "timing_distribution",
  pageActionPopupOpen: "timing_distribution",
  storageLocalGetJson: "timing_distribution",
  storageLocalSetJson: "timing_distribution",
  storageLocalGetIdb: "timing_distribution",
  storageLocalSetIdb: "timing_distribution",
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
export function getTrimmedString(str) {
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
 * Get a string representing the error which can be included in telemetry data.
 * If the resulting string is longer than 80 characters it is going to be
 * trimmed using the `getTrimmedString` helper function.
 *
 * @param {Error | DOMException | Components.Exception} error
 *        The error object to convert into a string representation.
 *
 * @returns {string}
 *          - The `error.name` string on DOMException or Components.Exception
 *            (trimmed to 80 chars).
 *          - "NoError" if error is falsey.
 *          - "UnkownError" as a fallback.
 */
export function getErrorNameForTelemetry(error) {
  let text = "UnknownError";
  if (!error) {
    text = "NoError";
  } else if (
    DOMException.isInstance(error) ||
    error instanceof Components.Exception
  ) {
    text = error.name;
    if (text.length > 80) {
      text = getTrimmedString(text);
    }
  }
  return text;
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
    this.gleanTimerIdsMap = new DefaultWeakMap(ext => new WeakMap());
  }

  // Stopwatch methods.
  stopwatchStart(extension, obj = extension) {
    this._wrappedStopwatchMethod("start", this.metric, extension, obj);
    this._wrappedTimingDistributionMethod("start", this.metric, extension, obj);
  }

  stopwatchFinish(extension, obj = extension) {
    this._wrappedStopwatchMethod("finish", this.metric, extension, obj);
    this._wrappedTimingDistributionMethod(
      "stopAndAccumulate",
      this.metric,
      extension,
      obj
    );
  }

  stopwatchCancel(extension, obj = extension) {
    this._wrappedStopwatchMethod("cancel", this.metric, extension, obj);
    this._wrappedTimingDistributionMethod(
      "cancel",
      this.metric,
      extension,
      obj
    );
  }

  // Histogram counters methods.
  histogramAdd(opts) {
    this._histogramAdd(this.metric, opts);
  }

  /**
   * Wraps a call to Glean timing_distribution methods for a given metric and extension.
   *
   * @param {string} method
   *        The Glean timing_distribution method to call ("start", "stopAndAccumulate" or "cancel").
   * @param {string} metric
   *        The Glean timing_distribution metric to record (used to retrieve the Glean metric type from the
   *        GLEAN_METRICS_TYPES map).
   * @param {Extension | BrowserExtensionContent} extension
   *        The extension to record the telemetry for.
   * @param {any | undefined} [obj = extension]
   *        An optional object the timing_distribution method call should be related to
   *        (defaults to the extension parameter when missing).
   */
  _wrappedTimingDistributionMethod(method, metric, extension, obj = extension) {
    if (!extension) {
      Cu.reportError(`Mandatory extension parameter is undefined`);
      return;
    }

    const gleanMetricType = GLEAN_METRICS_TYPES[metric];
    if (!gleanMetricType) {
      Cu.reportError(`Unknown metric ${metric}`);
      return;
    }

    if (gleanMetricType !== "timing_distribution") {
      Cu.reportError(
        `Glean metric ${metric} is of type ${gleanMetricType}, expected timing_distribution`
      );
      return;
    }

    switch (method) {
      case "start": {
        const timerId = Glean.extensionsTiming[metric].start();
        this.gleanTimerIdsMap.get(extension).set(obj, timerId);
        break;
      }
      case "stopAndAccumulate": // Intentional fall-through.
      case "cancel": {
        if (
          !this.gleanTimerIdsMap.has(extension) ||
          !this.gleanTimerIdsMap.get(extension).has(obj)
        ) {
          Cu.reportError(
            `timerId not found for Glean timing_distribution ${metric}`
          );
          return;
        }
        const timerId = this.gleanTimerIdsMap.get(extension).get(obj);
        this.gleanTimerIdsMap.get(extension).delete(obj);
        Glean.extensionsTiming[metric][method](timerId);
        break;
      }
      default:
        Cu.reportError(
          `Unknown method ${method} call for Glean metric ${metric}`
        );
    }
  }

  /**
   * Wraps a call to a TelemetryStopwatch method for a given metric and extension.
   *
   * @param {string} method
   *        The stopwatch method to call ("start", "finish" or "cancel").
   * @param {string} metric
   *        The stopwatch metric to record (used to retrieve the base histogram id from the HISTOGRAMS_IDS object).
   * @param {Extension | BrowserExtensionContent} extension
   *        The extension to record the telemetry for.
   * @param {any | undefined} [obj = extension]
   *        An optional telemetry stopwatch object (which defaults to the extension parameter when missing).
   */
  _wrappedStopwatchMethod(method, metric, extension, obj = extension) {
    if (!extension) {
      Cu.reportError(`Mandatory extension parameter is undefined`);
      return;
    }

    const baseId = HISTOGRAMS_IDS[metric];
    if (!baseId) {
      Cu.reportError(`Unknown metric ${metric}`);
      return;
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
   * @param {object}                              options
   * @param {Extension | BrowserExtensionContent} options.extension
   *        The extension to record the telemetry for.
   * @param {string | undefined}                  [options.category]
   *        An optional histogram category.
   * @param {number | undefined}                  [options.value]
   *        An optional value to record.
   */
  _histogramAdd(metric, { category, extension, value }) {
    if (!extension) {
      Cu.reportError(`Mandatory extension parameter is undefined`);
      return;
    }

    const baseId = HISTOGRAMS_IDS[metric];
    if (!baseId) {
      Cu.reportError(`Unknown metric ${metric}`);
      return;
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

    switch (GLEAN_METRICS_TYPES[metric]) {
      case "custom_distribution": {
        if (typeof category === "string") {
          Cu.reportError(
            `Unexpected unsupported category parameter set on Glean metric ${metric}`
          );
          return;
        }
        // NOTE: extensionsTiming may become a property of the GLEAN_METRICS_TYPES
        // map once we may introduce new histograms that are not part of the
        // extensionsTiming Glean metrics category.
        Glean.extensionsTiming[metric].accumulateSamples([value]);
        break;
      }
      case "labeled_counter": {
        if (typeof category !== "string") {
          Cu.reportError(
            `Missing mandatory category on adding data to labeled Glean metric ${metric}`
          );
          return;
        }
        Glean.extensionsCounters[metric][category].add(value ?? 1);
        break;
      }
      default:
        Cu.reportError(
          `Unexpected unsupported Glean metric type "${GLEAN_METRICS_TYPES[metric]}" for metric ${metric}`
        );
    }
  }
}

// Cache of the ExtensionTelemetryMetric instances that has been lazily created by the
// Extension Telemetry Proxy.
/** @type {Map<string|symbol, ExtensionTelemetryMetric>} */
const metricsCache = new Map();

/**
 * This proxy object provides the telemetry helpers for the currently supported metrics (the ones listed in
 * HISTOGRAMS_IDS), the telemetry helpers for a particular metric are lazily created
 * when the related property is being accessed on this object for the first time, e.g.:
 *
 *      ExtensionTelemetry.extensionStartup.stopwatchStart(extension);
 *      ExtensionTelemetry.browserActionPreloadResult.histogramAdd({category: "Shown", extension});
 */
export var ExtensionTelemetry = new Proxy(metricsCache, {
  get(target, prop, receiver) {
    // NOTE: if we would be start adding glean probes that do not have a unified
    // telemetry histogram counterpart, we would need to change this check
    // accordingly.
    if (!(prop in HISTOGRAMS_IDS)) {
      throw new Error(`Unknown metric ${String(prop)}`);
    }

    // Lazily create and cache the metric result object.
    if (!target.has(prop)) {
      target.set(prop, new ExtensionTelemetryMetric(prop));
    }

    return target.get(prop);
  },
});
