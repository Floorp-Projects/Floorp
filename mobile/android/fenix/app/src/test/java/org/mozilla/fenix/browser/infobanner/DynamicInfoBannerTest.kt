/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.spyk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DynamicInfoBannerTest {
    @Test
    fun `showBanner should set DynamicInfoBannerBehavior as behavior if scrollWithTopToolbar`() {
        val banner = spyk(
            DynamicInfoBanner(
                testContext, CoordinatorLayout(testContext), true, "", ""
            )
        )

        banner.showBanner()

        assertTrue((banner.binding.root.layoutParams as CoordinatorLayout.LayoutParams).behavior is DynamicInfoBannerBehavior)
    }

    @Test
    fun `showBanner should not set a behavior if not scrollWithTopToolbar`() {
        val banner = spyk(
            DynamicInfoBanner(
                testContext, CoordinatorLayout(testContext), false, "", ""
            )
        )

        banner.showBanner()

        assertNull((banner.binding.root.layoutParams as CoordinatorLayout.LayoutParams).behavior)
    }
}
