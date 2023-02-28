/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.locale

import android.content.Context
import android.view.ContextThemeWrapper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ActivityContextWrapperTest {
    @Test
    fun `WHEN a context has multiple wrappers THEN unwrap and return the original context`() {
        // Acts as the original activity context.
        val context: Context = testContext
        // First wrapper that holds the original without modification.
        val activityWrapper = ActivityContextWrapper(context)
        // Second wrapper that may have created a new context from the previous one.
        val themeWrapper = ContextThemeWrapper(activityWrapper, 0)

        val retrieved = ActivityContextWrapper.getOriginalContext(themeWrapper)

        assertEquals(context, retrieved)
        assertNotEquals(themeWrapper, retrieved)
    }

    @Test
    fun `WHEN there is no ActivityContextWrapper THEN return null`() {
        // Acts as the original activity context.
        val context: Context = testContext
        // A wrapper that may have created a new context from the previous one.
        val themeWrapper = ContextThemeWrapper(context, 0)

        val retrieved = ActivityContextWrapper.getOriginalContext(themeWrapper)
        val retrieved2 = ActivityContextWrapper.getOriginalContext(testContext)

        assertNull(retrieved)
        assertNull(retrieved2)
    }
}
