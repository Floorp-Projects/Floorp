/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.Context
import android.view.accessibility.AccessibilityManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows.shadowOf
import org.robolectric.shadows.ShadowAccessibilityManager

@RunWith(AndroidJUnit4::class)
class ContextKtTest {

    lateinit var accessibilityManager: ShadowAccessibilityManager

    @Before
    fun setUp() {
        accessibilityManager = shadowOf(
            testContext
                .getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager,
        )
    }

    @Test
    fun `screen reader enabled`() {
        // Given
        accessibilityManager.setTouchExplorationEnabled(true)

        // When
        val isEnabled = testContext.isScreenReaderEnabled

        // Then
        assertTrue(isEnabled)
    }

    @Test
    fun `screen reader disabled`() {
        // Given
        accessibilityManager.setTouchExplorationEnabled(false)

        // When
        val isEnabled = testContext.isScreenReaderEnabled

        // Then
        assertFalse(isEnabled)
    }
}
