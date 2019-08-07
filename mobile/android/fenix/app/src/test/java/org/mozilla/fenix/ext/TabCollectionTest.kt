/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.core.content.ContextCompat
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class TabCollectionTest {

    @Test
    fun getIconColor() {
        val color = mockTabCollection(100L).getIconColor(testContext)
        // Color does not change
        for (i in 0..99) {
            assertEquals(color, mockTabCollection(100L).getIconColor(testContext))
        }

        // Returns a color for negative IDs
        val defaultColor = ContextCompat.getColor(testContext, R.color.white_color)
        assertNotEquals(defaultColor, mockTabCollection(-123L).getIconColor(testContext))
    }

    private fun mockTabCollection(id: Long): TabCollection {
        val collection: TabCollection = mock()
        `when`(collection.id).thenReturn(id)
        return collection
    }
}
