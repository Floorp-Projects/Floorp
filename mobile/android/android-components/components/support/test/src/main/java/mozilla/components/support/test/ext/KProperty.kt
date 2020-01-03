/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.ext

import kotlin.reflect.KProperty0
import kotlin.reflect.jvm.isAccessible

/**
 * Returns true if a lazy property has been initialized, or if the property is not lazy.
 *
 * implementation inspired by https://stackoverflow.com/a/42536189
 */
val KProperty0<*>.isLazyInitialized: Boolean
    get() {
        // Prevent exception for accessing private getDelegate function.
        val originalAccessLevel = isAccessible
        isAccessible = true

        val lazyDelegate = getDelegate()
        require(lazyDelegate is Lazy<*>) { "Expected receiver property to be lazy" }

        val isLazyInitialized = lazyDelegate.isInitialized()

        // Reset access level.
        isAccessible = originalAccessLevel
        return isLazyInitialized
    }
