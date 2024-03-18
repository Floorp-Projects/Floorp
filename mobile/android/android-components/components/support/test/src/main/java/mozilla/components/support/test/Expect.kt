/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test

import org.junit.Assert.fail
import kotlin.reflect.KClass

/**
 * Expect [block] to throw an exception. Otherwise fail the test (junit).
 */
inline fun <reified T : Throwable> expectException(clazz: KClass<T>, block: () -> Unit) {
    try {
        block()
        fail("Expected exception to be thrown: $clazz")
    } catch (e: Throwable) {
        if (e !is T) {
            throw e
        }
    }
}
