/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import kotlin.reflect.full.createInstance

@RunWith(Parameterized::class)
class PermissionTest<out T : Permission>(private val permission: T) {
    @Test
    fun `GIVEN a permission WHEN asking for it's default id THEN return permission's class name`() {
        assertEquals(permission::class.java.simpleName, permission.id)
    }

    @Test
    fun `GIVEN a permission WHEN asking for it's name THEN return permission's class name`() {
        assertEquals(permission::class.java.simpleName, permission.name)
    }

    companion object {
        @JvmStatic
        @Parameterized.Parameters(name = "{0}")
        fun permissions() = Permission::class.sealedSubclasses.map {
            it.createInstance()
        }
    }
}
