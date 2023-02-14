/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.toolbar.BrowserToolbar
import org.junit.Assert.assertEquals
import org.junit.Test

class DynamicInfoBannerBehaviorTest {
    @Test
    fun `layoutDependsOn should not do anything if not for BrowserToolbar as a dependency`() {
        val behavior = spyk(DynamicInfoBannerBehavior(mockk(), null))

        behavior.layoutDependsOn(mockk(), mockk(), mockk())

        verify(exactly = 0) { behavior.toolbarHeight }
        verify(exactly = 0) { behavior.toolbarHeight = any() }
        verify(exactly = 0) { behavior.setBannerYTranslation(any(), any()) }
    }

    @Test
    fun `layoutDependsOn should update toolbarHeight and translate the banner`() {
        val behavior = spyk(DynamicInfoBannerBehavior(mockk(), null))
        val banner: View = mockk(relaxed = true)
        val toolbar: BrowserToolbar = mockk {
            every { height } returns 99
            every { translationY } returns -33f
        }

        assertEquals(0, behavior.toolbarHeight)

        behavior.layoutDependsOn(mockk(), banner, toolbar)

        assertEquals(99, behavior.toolbarHeight)
        verify { behavior.setBannerYTranslation(banner, -33f) }
    }

    @Test
    fun `onDependentViewChanged should translate the banner`() {
        val behavior = spyk(DynamicInfoBannerBehavior(mockk(), null))
        val banner: View = mockk(relaxed = true)
        val toolbar: BrowserToolbar = mockk {
            every { height } returns 50
            every { translationY } returns -23f
        }

        behavior.layoutDependsOn(mockk(), banner, toolbar)

        verify { behavior.setBannerYTranslation(banner, -23f) }
    }

    @Test
    fun `setBannerYTranslation should set banner translation to be toolbarHeight + it's translation`() {
        val behavior = spyk(DynamicInfoBannerBehavior(mockk(), null))
        val banner: View = mockk(relaxed = true)
        behavior.toolbarHeight = 30

        behavior.setBannerYTranslation(banner, -20f)

        verify { banner.translationY = 10f }
    }
}
