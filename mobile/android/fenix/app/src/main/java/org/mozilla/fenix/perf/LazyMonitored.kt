/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.atomic.AtomicInteger

private val logger = Logger("LazyMonitored")

/**
 * A container for the number of components initialized.
 */
object ComponentInitCount {
    val count = AtomicInteger(0)
}

/**
 * A convenience function for setting the [LazyMonitored] property delegate, which wraps
 * [lazy] to add performance monitoring.
 */
fun <T> lazyMonitored(initializer: () -> T) = LazyMonitored(initializer)

/**
 * A wrapper around the [lazy] property delegate to monitor for performance related issues.
 * For example, we can count the number of components initialized to see how the number of
 * components initialized on start up impacts start up time.
 */
class LazyMonitored<T>(initializer: () -> T) {
    // Lazy is thread safe.
    private val lazyValue = lazy {
        // We're unlikely to have 4 billion components so we don't handle overflow.
        val componentInitCount = ComponentInitCount.count.incrementAndGet()

        initializer().also {
            @Suppress("UNNECESSARY_NOT_NULL_ASSERTION") // the compiler fails with !! but warns with !!.
            val className = if (it == null) "null" else it!!::class.java.canonicalName
            logger.debug("Init component #$componentInitCount: $className")
        }
    }

    operator fun getValue(thisRef: Any?, property: KProperty<*>): T = lazyValue.value
}
