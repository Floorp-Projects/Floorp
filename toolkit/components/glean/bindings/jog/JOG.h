/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_JOG_h
#define mozilla_glean_JOG_h

namespace mozilla::glean {

class JOG {
 public:
  /**
   * Returns whether JOG knows about a category by this name
   *
   * @param aCategoryName The category name to check.
   *
   * @returns true if JOG is aware of a category by the given name at this time
   */
  static bool HasCategory(const nsACString& aCategoryName);

  /**
   * Runs the runtime registrar.
   *
   * Locates the runtime metrics file and, if present, loads and processes it.
   *
   * **Note:** This is expensive, running synchronous file I/O to ensure that
   * the registration is complete when this calls returns.
   *
   * @param aForce Set to `true` if you want to force the I/O to run. Defaults
   *        to `false`, which doesn't run the I/O if it's already run and
   *        returns the previous return value.
   * @returns whether it found the runtime metrics file and succesfully loaded,
   *          processed, and registered the described metrics.
   */
  static bool EnsureRuntimeMetricsRegistered(bool aForce = false);

  /**
   * Returns whether, if a metric is absent in the runtime-registered metrics,
   * you should check the compile-time-registered metrics.
   *
   * Runtime-registered metrics can either replace all compile-time-registered
   * metrics (like in artefact builds) or just be supplementing compile-time-
   * registered metrics (like addons/dynamic telemetry/etc).
   *
   * This is tied to the current state of runtime metric registration. So it
   * may return false at one time and true later (e.g. if RuntimeRegistrar is
   * run in between).
   *
   * @return true if you should treat the runtime-registered metrics as
   *         authoritative and comprehensive.
   */
  static bool AreRuntimeMetricsComprehensive();

  /**
   * Adds the runtime-registered metrics' categories to `aNames`.
   *
   * @param aNames The list to add the categories' names to.
   */
  static void GetCategoryNames(nsTArray<nsString>& aNames);

  /**
   * Get the metric id+type in a u32 for a named runtime-registered metric.
   *
   * Return value's only useful to GleanJSMetricsLookup.h
   *
   * @param aMetricName The `myCategory.myName` dotted.camelCase metric name.
   * @return Nothing() if no metric by that name was registered at runtime.
   *         Otherwise, the encoded u32 with metric id and metric type id for
   *         the runtime-registered metric.
   */
  static Maybe<uint32_t> GetMetric(const nsACString& aMetricName);
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_JOG_h */
