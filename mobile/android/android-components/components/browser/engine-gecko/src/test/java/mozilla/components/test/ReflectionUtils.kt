/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.test

import java.security.AccessController
import java.security.PrivilegedExceptionAction

object ReflectionUtils {
    fun <T : Any> setField(instance: T, fieldName: String, value: Any?) {
        val mapField = AccessController.doPrivileged(
            PrivilegedExceptionAction {
                try {
                    val field = instance::class.java.getField(fieldName)
                    field.isAccessible = true
                    return@PrivilegedExceptionAction field
                } catch (e: ReflectiveOperationException) {
                    throw Error(e)
                }
            },
        )

        mapField.set(instance, value)
    }
}
