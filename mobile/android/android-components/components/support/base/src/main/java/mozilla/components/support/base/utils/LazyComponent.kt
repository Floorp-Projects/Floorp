/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.utils

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.atomic.AtomicInteger

private val logger = Logger("LazyComponent")

/**
 * A wrapper around the [lazy] property delegate that is intended for use with application-wide
 * components (such as services in the Service Locator pattern or dependencies in the Dependency
 * Injection pattern).
 *
 * This class helps address the issue where kotlin's built-in [lazy] property delegate does not
 * handle recursive initialization by inserting a getter between the lazy reference. For example, in:
 * ```
 * val useCases by lazy { UseCases(...) }
 * val search by lazy { Search(useCases) }
 * ```
 * When `search` is referenced, it will initialized and **so will `useCases`**.
 * This is a problem if there are many dependencies or they are expensive. [LazyComponent]
 * addresses this issue by allowing the class to be referenced without being initialized. To apply
 * this to the example above:
 * ```
 * val useCases = LazyComponent { UseCases(...) }
 * val search = LazyComponent { Search(useCases) }
 * ```
 *
 * To call methods on the class and thus initialize it, one would call `LazyComponent.get()`, e.g.
 * `search.get().startSearch(terms)`.
 *
 * This class also adds performance monitoring code to component initialization that, when paired
 * with testing, can help prevent component initialization at inopportune times.
 */
class LazyComponent<T>(initializer: () -> T) {
    // Lazy is thread safe.
    private val lazyValue: Lazy<T> = lazy {
        val initCount = initCount.incrementAndGet() // See property kdoc with regard to overflow.

        initializer().also {
            @Suppress("UNNECESSARY_NOT_NULL_ASSERTION") // the compiler fails with !! but warns with !!.
            val className = if (it == null) "null" else it!!::class.java.canonicalName
            logger.debug("Initialized lazyComponent #$initCount: $className")
        }
    }

    /**
     * Returns the component, initializing it if it has not already been initialized.
     *
     * To ensure this value is initialized lazily, it is expected that this method will only be
     * called when the class instance needs to be interacted with, i.e. the return value should not
     * be stored in a member property during an initializer.
     */
    fun get(): T = lazyValue.value

    /**
     * Returns whether or not the component has been initialized yet.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun isInitialized(): Boolean = lazyValue.isInitialized()

    companion object {
        /**
         * The number of [LazyComponent]s initialized. This is intended to be checked during testing.
         * For example, a team can run a test scenario that starts the app and, if the [initCount]
         * increases from main, the team can fail the test to alert that a new component is initialized.
         * This can help the team catch cases where they didn't mean to initialize new components or add
         * new code on start up, keeping it performant.
         *
         * This class assumes the app will not have 4 billion+ components so we don't handle overflow.
         */
        @VisibleForTesting(otherwise = PRIVATE)
        val initCount = AtomicInteger(0)
    }
}
