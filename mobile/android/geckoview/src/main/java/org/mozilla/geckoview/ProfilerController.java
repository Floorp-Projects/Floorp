/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import org.mozilla.gecko.GeckoJavaSampler;

/**
 * ProfilerController is used to manage GeckoProfiler related features.
 *
 * <p>If you want to add a profiler marker to mark a point in time (without a duration) you can
 * directly use <code>profilerController.addMarker("marker name")</code>. Or if you want to provide
 * more information, you can use <code>
 * profilerController.addMarker("marker name", "extra information")</code> If you want to add a
 * profiler marker with a duration (with start and end time) you can use it like this: <code>
 *     Double startTime = profilerController.getProfilerTime();
 *     ...some code you want to measure...
 *     profilerController.addMarker("name", startTime);
 * </code> Or you can capture start and end time in somewhere, then add the marker in somewhere
 * else: <code>
 *     Double startTime = profilerController.getProfilerTime();
 *     ...some code you want to measure (or end time can be collected in a callback)...
 *     Double endTime = profilerController.getProfilerTime();
 *
 *     ...somewhere else in the codebase...
 *     profilerController.addMarker("name", startTime, endTime);
 * </code> Here's an <code>addMarker</code> example with all the possible parameters: <code>
 *     Double startTime = profilerController.getProfilerTime();
 *     ...some code you want to measure...
 *     Double endTime = profilerController.getProfilerTime();
 *
 *     ...somewhere else in the codebase...
 *     profilerController.addMarker("name", startTime, endTime, "extra information");
 * </code> <code>isProfilerActive</code> method is handy when you want to get more information to
 * add inside the marker, but you think it's going to be computationally heavy (and useless) when
 * profiler is not running:
 *
 * <pre>
 * <code>
 *     Double startTime = profilerController.getProfilerTime();
 *     ...some code you want to measure...
 *     if (profilerController.isProfilerActive()) {
 *         String info = aFunctionYouDoNotWantToCallWhenProfilerIsNotActive();
 *         profilerController.addMarker("name", startTime, info);
 *     }
 * </code>
 * </pre>
 *
 * FIXME(bug 1618560): Currently only works in the main thread.
 */
@UiThread
public class ProfilerController {
  private static final String LOGTAG = "ProfilerController";

  /**
   * Returns true if profiler is active and it's allowed the add markers. It's useful when it's
   * computationally heavy to get startTime or the additional text for the marker. That code can be
   * wrapped with isProfilerActive if check to reduce the overhead of it.
   *
   * @return true if profiler is active and safe to add a new marker.
   */
  public boolean isProfilerActive() {
    return GeckoJavaSampler.isProfilerActive();
  }

  /**
   * Get the profiler time to be able to mark the start of the marker events. can be used like this:
   * <code>
   *     Double startTime = profilerController.getProfilerTime();
   *     ...some code you want to measure...
   *     profilerController.addMarker("name", startTime);
   * </code>
   *
   * @return profiler time as double or null if the profiler is not active.
   */
  public @Nullable Double getProfilerTime() {
    return GeckoJavaSampler.tryToGetProfilerTime();
  }

  /**
   * Add a profiler marker to Gecko Profiler with the given arguments. No-op if profiler is not
   * active.
   *
   * @param aMarkerName Name of the event as a string.
   * @param aStartTime Start time as Double. It can be null if you want to mark a point of time.
   * @param aEndTime End time as Double. If it's null, this function implicitly gets the end time.
   * @param aText An optional string field for more information about the marker.
   */
  public void addMarker(
      @NonNull final String aMarkerName,
      @Nullable final Double aStartTime,
      @Nullable final Double aEndTime,
      @Nullable final String aText) {
    GeckoJavaSampler.addMarker(aMarkerName, aStartTime, aEndTime, aText);
  }

  /**
   * Add a profiler marker to Gecko Profiler with the given arguments. End time will be added
   * automatically with the current profiler time when the function is called. No-op if profiler is
   * not active. This is an overload of {@link #addMarker(String, Double, Double, String)} for
   * convenience.
   *
   * @param aMarkerName Name of the event as a string.
   * @param aStartTime Start time as Double. It can be null if you want to mark a point of time.
   * @param aText An optional string field for more information about the marker.
   */
  public void addMarker(
      @NonNull final String aMarkerName,
      @Nullable final Double aStartTime,
      @Nullable final String aText) {
    GeckoJavaSampler.addMarker(aMarkerName, aStartTime, null, aText);
  }

  /**
   * Add a profiler marker to Gecko Profiler with the given arguments. End time will be added
   * automatically with the current profiler time when the function is called. No-op if profiler is
   * not active. This is an overload of {@link #addMarker(String, Double, Double, String)} for
   * convenience.
   *
   * @param aMarkerName Name of the event as a string.
   * @param aStartTime Start time as Double. It can be null if you want to mark a point of time.
   */
  public void addMarker(@NonNull final String aMarkerName, @Nullable final Double aStartTime) {
    addMarker(aMarkerName, aStartTime, null, null);
  }

  /**
   * Add a profiler marker to Gecko Profiler with the given arguments. Time will be added
   * automatically with the current profiler time when the function is called. No-op if profiler is
   * not active. This is an overload of {@link #addMarker(String, Double, Double, String)} for
   * convenience.
   *
   * @param aMarkerName Name of the event as a string.
   * @param aText An optional string field for more information about the marker.
   */
  public void addMarker(@NonNull final String aMarkerName, @Nullable final String aText) {
    addMarker(aMarkerName, null, null, aText);
  }

  /**
   * Add a profiler marker to Gecko Profiler with the given arguments. Time will be added
   * automatically with the current profiler time when the function is called. No-op if profiler is
   * not active. This is an overload of {@link #addMarker(String, Double, Double, String)} for
   * convenience.
   *
   * @param aMarkerName Name of the event as a string.
   */
  public void addMarker(@NonNull final String aMarkerName) {
    addMarker(aMarkerName, null, null, null);
  }

  /**
   * Start the Gecko profiler with the given settings. This is used by embedders which want to
   * control the profiler from the embedding app. This allows them to provide an easier access point
   * to profiling, as an alternative to the traditional way of using a desktop Firefox instance
   * connected via USB + adb.
   *
   * @param aFilters The list of threads to profile, as an array of string of thread names filters.
   *     Each filter is used as a case-insensitive substring match against the actual thread names.
   * @param aFeaturesArr The list of profiler features to enable for profiling, as a string array.
   */
  public void startProfiler(
      @NonNull final String[] aFilters, @NonNull final String[] aFeaturesArr) {
    GeckoJavaSampler.startProfiler(aFilters, aFeaturesArr);
  }

  /**
   * Stop the profiler and capture the recorded profile. This method is asynchronous.
   *
   * @return GeckoResult for the captured profile. The profile is returned as a byte[] buffer
   *     containing a gzip-compressed payload (with gzip header) of the profile JSON.
   */
  public @NonNull GeckoResult<byte[]> stopProfiler() {
    return GeckoJavaSampler.stopProfiler();
  }
}
