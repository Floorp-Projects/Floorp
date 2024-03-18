/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

/**
 * A function which wraps [lazy].
 *
 * This functionality was previously used to add performance monitoring. This
 * wrapper could be useful in the future to add more monitoring. Even though
 * this method is unused, we keep the code because re-adding this wrapper to
 * every component is non-trivial.
 */
fun <T> lazyMonitored(initializer: () -> T): Lazy<T> = lazy(initializer)
