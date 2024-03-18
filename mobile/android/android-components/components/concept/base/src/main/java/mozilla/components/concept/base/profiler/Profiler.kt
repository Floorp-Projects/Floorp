/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.profiler

/**
 * [Profiler] is being used to manage Firefox Profiler related features.
 *
 * If you want to add a profiler marker to mark a point in time (without a duration)
 * you can directly use `engine.profiler?.addMarker("marker name")`.
 * Or if you want to provide more information, you can use
 * `engine.profiler?.addMarker("marker name", "extra information")`.
 *
 * If you want to add a profiler marker with a duration (with start and end time)
 * you can use it like this, it will automatically get the end time inside the addMarker:
 * ```
 *     val startTime = engine.profiler?.getProfilerTime()
 *     ...some code you want to measure...
 *     engine.profiler?.addMarker("name", startTime)
 * ```
 *
 * Or you can capture start and end time in somewhere, then add the marker in somewhere else:
 * ```
 *     val startTime = engine.profiler?.getProfilerTime()
 *     ...some code you want to measure (or end time can be collected in a callback)...
 *     val endTime = engine.profiler?.getProfilerTime()
 *
 *     ...somewhere else in the codebase...
 *     engine.profiler?.addMarker("name", startTime, endTime)
 * ```
 *
 * Here's an [Profiler.addMarker] example with all the possible parameters:
 * ```
 *     val startTime = engine.profiler?.getProfilerTime()
 *     ...some code you want to measure...
 *     val endTime = engine.profiler?.getProfilerTime()
 *
 *     ...somewhere else in the codebase...
 *     engine.profiler?.addMarker("name", startTime, endTime, "extra information")
 * ```
 *
 * [Profiler.isProfilerActive] method is handy when you want to get more information to
 * add inside the marker, but you think it's going to be computationally heavy (and useless)
 * when profiler is not running:
 * ```
 *     val startTime = engine.profiler?.getProfilerTime()
 *     ...some code you want to measure...
 *     if (engine.profiler?.isProfilerActive()) {
 *         val info = aFunctionYouDoNotWantToCallWhenProfilerIsNotActive()
 *         engine.profiler?.addMarker("name", startTime, info)
 *     }
 * ```
 */
interface Profiler {
    /**
     * Returns true if profiler is active and it's allowed the add markers.
     * It's useful when it's computationally heavy to get startTime or the
     * additional text for the marker. That code can be wrapped with
     * isProfilerActive if check to reduce the overhead of it.
     *
     * @return true if profiler is active and safe to add a new marker.
     */
    fun isProfilerActive(): Boolean

    /**
     * Get the profiler time to be able to mark the start of the marker events.
     * can be used like this:
     *
     * <code>
     *     val startTime = engine.profiler?.getProfilerTime()
     *     ...some code you want to measure...
     *     engine.profiler?.addMarker("name", startTime)
     * </code>
     *
     * @return profiler time as Double or null if the profiler is not active.
     */
    fun getProfilerTime(): Double?

    /**
     * Add a profiler marker to Gecko Profiler with the given arguments.
     * It can be used for either adding a point-in-time marker or a duration marker.
     * No-op if profiler is not active.
     *
     * @param markerName Name of the event as a string.
     * @param startTime Start time as Double. It can be null if you want to mark a point of time.
     * @param endTime End time as Double. If it's null, this function implicitly gets the end time.
     * @param text An optional string field for more information about the marker.
     */
    fun addMarker(markerName: String, startTime: Double?, endTime: Double?, text: String?)

    /**
     * Add a profiler marker to Gecko Profiler with the given arguments.
     * End time will be added automatically with the current profiler time when the function is called.
     * No-op if profiler is not active.
     * This is an overload of [Profiler.addMarker] for convenience.
     *
     * @param aMarkerName Name of the event as a string.
     * @param aStartTime Start time as Double. It can be null if you want to mark a point of time.
     * @param aText An optional string field for more information about the marker.
     */
    fun addMarker(markerName: String, startTime: Double?, text: String?)

    /**
     * Add a profiler marker to Gecko Profiler with the given arguments.
     * End time will be added automatically with the current profiler time when the function is called.
     * No-op if profiler is not active.
     * This is an overload of [Profiler.addMarker] for convenience.
     *
     * @param markerName Name of the event as a string.
     * @param startTime Start time as Double. It can be null if you want to mark a point of time.
     */
    fun addMarker(markerName: String, startTime: Double?)

    /**
     * Add a profiler marker to Gecko Profiler with the given arguments.
     * Time will be added automatically with the current profiler time when the function is called.
     * No-op if profiler is not active.
     * This is an overload of [Profiler.addMarker] for convenience.
     *
     * @param markerName Name of the event as a string.
     * @param text An optional string field for more information about the marker.
     */
    fun addMarker(markerName: String, text: String?)

    /**
     * Add a profiler marker to Gecko Profiler with the given arguments.
     * Time will be added automatically with the current profiler time when the function is called.
     * No-op if profiler is not active.
     * This is an overload of [Profiler.addMarker] for convenience.
     *
     * @param markerName Name of the event as a string.
     */
    fun addMarker(markerName: String)

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
    fun startProfiler(filters: Array<String>, features: Array<String>)

    /**
     * Stop the profiler and capture the recorded profile. This method is asynchronous.
     *
     * @return GeckoResult for the captured profile. The profile is returned as a byte[] buffer
     *     containing a gzip-compressed payload (with gzip header) of the profile JSON.
     */
    fun stopProfiler(onSuccess: (ByteArray?) -> Unit, onError: (Throwable) -> Unit)
}
