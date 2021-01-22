/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.view.animation.DecelerateInterpolator
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserToolbarYTranslationStrategyTest {
    @Test
    fun `snapAnimator should use a DecelerateInterpolator with SNAP_ANIMATION_DURATION for bottom toolbar translations`() {
        val strategy = BottomToolbarBehaviorStrategy()

        assertTrue(strategy.animator.interpolator is DecelerateInterpolator)
        assertEquals(SNAP_ANIMATION_DURATION, strategy.animator.duration)
    }

    @Test
    fun `snapAnimator should use a DecelerateInterpolator with SNAP_ANIMATION_DURATION for top toolbar translations`() {
        val strategy = TopToolbarBehaviorStrategy()

        assertTrue(strategy.animator.interpolator is DecelerateInterpolator)
        assertEquals(SNAP_ANIMATION_DURATION, strategy.animator.duration)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy should start with isToolbarExpanding = false`() {
        val strategy = BottomToolbarBehaviorStrategy()

        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `TopToolbarBehaviorStrategy should start with isToolbarExpanding = false`() {
        val strategy = TopToolbarBehaviorStrategy()

        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapWithAnimation should collapse toolbar if more than half hidden`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(50f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(51f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(100f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(333f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        verify(strategy, times(4)).collapseWithAnimation(toolbar)
        verify(strategy, never()).expandWithAnimation(toolbar)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapWithAnimation should expand toolbar if more than half visible`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(49f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(0f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(-50f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        verify(strategy, times(3)).expandWithAnimation(toolbar)
        verify(strategy, never()).collapseWithAnimation(toolbar)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapWithAnimation should collapse toolbar if more than half hidden`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-51f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(-100f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(-333f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        verify(strategy, times(3)).collapseWithAnimation(toolbar)
        verify(strategy, never()).expandWithAnimation(toolbar)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapWithAnimation should expand toolbar if more than half visible`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-50f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(-49f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(0f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        doReturn(50f).`when`(toolbar).translationY
        strategy.snapWithAnimation(toolbar)

        verify(strategy, times(4)).expandWithAnimation(toolbar)
        verify(strategy, never()).collapseWithAnimation(toolbar)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should end translations animations if in progress`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()

        strategy.snapImmediately(toolbar)

        verify(animator).end()
        verify(toolbar, never()).translationY
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if half translated`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(50f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if more than half`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(55f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if translated off screen`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(555f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated less than half`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(49f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated 0`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(0f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated inside the screen`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-1f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should end translations animations if in progress`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()

        strategy.snapImmediately(toolbar)

        verify(animator).end()
        verify(toolbar, never()).translationY
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate translate to 0 the toolbar if translated less than half`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-49f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated 0`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(0f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated inside the screen`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(1f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if half translated`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-50f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if more than half translated`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-55f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = -100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated offscreen`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        doReturn(-111f).`when`(toolbar).translationY
        strategy.snapImmediately(toolbar)
        verify(toolbar).translationY = -100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - expandWithAnimation should translate the toolbar to to y 0`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()

        strategy.expandWithAnimation(toolbar)

        verify(strategy).animateToTranslationY(toolbar, 0f)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - expandWithAnimation should translate the toolbar to to y 0`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()

        strategy.expandWithAnimation(toolbar)

        verify(strategy).animateToTranslationY(toolbar, 0f)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should expand toolbar`() {
        // Setting the scenario in which forceExpandWithAnimation will actually do what the name says.
        // Below this test we can change each variable one at a time to test them in isolation.

        val strategy = spy(BottomToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator).cancel()
        verify(strategy).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not force expand the toolbar if not currently collapsing`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        strategy.wasLastExpanding = true
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not expand if user swipes down`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, 100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not expand the toolbar if it is already expanded`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(0f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should expand toolbar`() {
        // Setting the scenario in which forceExpandWithAnimation will actually do what the name says.
        // Below this test we can change each variable one at a time to test them in isolation.

        val strategy = spy(TopToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(-100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator).cancel()
        verify(strategy).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not force expand the toolbar if not currently collapsing`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        strategy.wasLastExpanding = true
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(-100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not expand if user swipes up`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(-100f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, 10f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not expand the toolbar if it is already expanded`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val toolbar: BrowserToolbar = mock()
        doReturn(0f).`when`(toolbar).translationY

        strategy.forceExpandWithAnimation(toolbar, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - collapseWithAnimation should animate translating the toolbar down, off-screen`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        strategy.collapseWithAnimation(toolbar)

        verify(strategy).animateToTranslationY(toolbar, 100f)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - collapseWithAnimation should animate translating the toolbar up, off-screen`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height

        strategy.collapseWithAnimation(toolbar)

        verify(strategy).animateToTranslationY(toolbar, -100f)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should translate up the toolbar with the distance parameter`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(50f).`when`(toolbar).translationY

        strategy.translate(toolbar, -25f)

        verify(toolbar).translationY = 25f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should translate down the toolbar with the distance parameter`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(50f).`when`(toolbar).translationY

        strategy.translate(toolbar, 25f)

        verify(toolbar).translationY = 75f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate up the toolbar if already expanded`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(0f).`when`(toolbar).translationY

        strategy.translate(toolbar, -1f)

        verify(toolbar).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate up the toolbar more than to 0`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(50f).`when`(toolbar).translationY

        strategy.translate(toolbar, -51f)

        verify(toolbar).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate down the toolbar if already collapsed`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(100f).`when`(toolbar).translationY

        strategy.translate(toolbar, 1f)

        verify(toolbar).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate down the toolbar more than it's height`() {
        val strategy = BottomToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(50f).`when`(toolbar).translationY

        strategy.translate(toolbar, 51f)

        verify(toolbar).translationY = 100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should translate down the toolbar with the distance parameter`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(-50f).`when`(toolbar).translationY

        strategy.translate(toolbar, 25f)

        verify(toolbar).translationY = -75f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should translate up the toolbar with the distance parameter`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(-50f).`when`(toolbar).translationY

        strategy.translate(toolbar, 25f)

        verify(toolbar).translationY = -75f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate down the toolbar if already expanded`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(0f).`when`(toolbar).translationY

        strategy.translate(toolbar, -1f)

        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate down the toolbar more than to 0`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(-50f).`when`(toolbar).translationY

        strategy.translate(toolbar, -51f)

        verify(toolbar).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate up the toolbar if already collapsed`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(-100f).`when`(toolbar).translationY

        strategy.translate(toolbar, 1f)

        verify(toolbar).translationY = -100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate up the toolbar more than it's height`() {
        val strategy = TopToolbarBehaviorStrategy()
        val toolbar: BrowserToolbar = mock()
        doReturn(100).`when`(toolbar).height
        doReturn(-50f).`when`(toolbar).translationY

        strategy.translate(toolbar, 51f)

        verify(toolbar).translationY = -100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - animateToTranslationY should set wasLastExpanding if expanding`() {
        val strategy = BottomToolbarBehaviorStrategy()
        strategy.wasLastExpanding = false
        val toolbar: BrowserToolbar = mock()
        doReturn(50f).`when`(toolbar).translationY

        strategy.animateToTranslationY(toolbar, 10f)
        assertTrue(strategy.wasLastExpanding)

        strategy.animateToTranslationY(toolbar, 60f)
        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - animateToTranslationY should animate to the indicated y translation`() {
        val strategy = spy(BottomToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val toolbar: BrowserToolbar = BrowserToolbar(testContext)
        val animator: ValueAnimator = spy(strategy.animator)
        strategy.animator = animator

        strategy.animateToTranslationY(toolbar, 10f)

        verify(animator).start()
        animator.end()
        assertEquals(10f, toolbar.translationY)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - animateToTranslationY should set wasLastExpanding if expanding`() {
        val strategy = TopToolbarBehaviorStrategy()
        strategy.wasLastExpanding = false
        val toolbar: BrowserToolbar = mock()
        doReturn(-50f).`when`(toolbar).translationY

        strategy.animateToTranslationY(toolbar, -10f)
        assertTrue(strategy.wasLastExpanding)

        strategy.animateToTranslationY(toolbar, -60f)
        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - animateToTranslationY should animate to the indicated y translation`() {
        val strategy = spy(TopToolbarBehaviorStrategy())
        strategy.wasLastExpanding = false
        val toolbar: BrowserToolbar = BrowserToolbar(testContext)
        val animator: ValueAnimator = spy(strategy.animator)
        strategy.animator = animator

        strategy.animateToTranslationY(toolbar, -10f)

        verify(animator).start()
        animator.end()
        assertEquals(-10f, toolbar.translationY)
    }
}
