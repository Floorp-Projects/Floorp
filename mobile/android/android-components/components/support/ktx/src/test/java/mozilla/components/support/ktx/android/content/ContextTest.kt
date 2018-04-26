/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.Context
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class ContextTest {
    @Test
    fun `systemService() returns same service as getSystemService()`() {
        val context = RuntimeEnvironment.application

        assertEquals(
            context.getSystemService(Context.INPUT_METHOD_SERVICE),
            context.systemService(Context.INPUT_METHOD_SERVICE))

        assertEquals(
            context.getSystemService(Context.ACTIVITY_SERVICE),
            context.systemService(Context.ACTIVITY_SERVICE))

        assertEquals(
            context.getSystemService(Context.LOCATION_SERVICE),
            context.systemService(Context.LOCATION_SERVICE))
    }
}
