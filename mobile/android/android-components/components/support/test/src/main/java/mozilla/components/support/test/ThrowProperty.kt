/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test

import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

/**
 * A [ReadWriteProperty] implementation for creating stub properties.
 */
class ThrowProperty<T> : ReadWriteProperty<Any, T> {
    override fun getValue(thisRef: Any, property: KProperty<*>): T =
        throw UnsupportedOperationException("Cannot get $property")

    override fun setValue(thisRef: Any, property: KProperty<*>, value: T) =
        throw UnsupportedOperationException("Cannot set $property")
}
