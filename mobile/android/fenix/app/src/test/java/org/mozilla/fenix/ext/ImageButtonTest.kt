/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.view.View
import mozilla.components.support.test.robolectric.testContext
import org.mozilla.fenix.TestApplication
import org.junit.Test
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertEquals
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import android.widget.ImageButton

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class ImageButtonTest {
    private val imageButton = ImageButton(testContext)

    @Test
    fun `Hide and disable`() {
        imageButton.hideAndDisable()
        assertFalse(imageButton.isEnabled)
        assertEquals(View.INVISIBLE, imageButton.visibility)
    }

    @Test
    fun `Show and enable`() {
        imageButton.showAndEnable()
        assertTrue(imageButton.isEnabled)
        assertEquals(View.VISIBLE, imageButton.visibility)
    }
}
