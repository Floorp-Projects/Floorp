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
