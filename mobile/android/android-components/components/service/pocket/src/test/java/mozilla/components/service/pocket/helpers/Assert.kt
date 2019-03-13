/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

import org.junit.Assert.assertEquals
import kotlin.reflect.KClass
import kotlin.reflect.KVisibility

fun <T : Any> assertConstructorsVisibility(assertedClass: KClass<T>, visibility: KVisibility) {
    assertedClass.constructors.forEach {
        assertEquals(visibility, it.visibility)
    }
}
