/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.animation.ValueAnimator
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.test.ext.junit.runners.AndroidJUnit4
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
class ViewYTranslationStrategyTest {
    @Test
    fun `snapAnimator should use a DecelerateInterpolator with SNAP_ANIMATION_DURATION for bottom toolbar translations`() {
        val strategy = BottomViewBehaviorStrategy()

        assertTrue(strategy.animator.interpolator is DecelerateInterpolator)
        assertEquals(SNAP_ANIMATION_DURATION, strategy.animator.duration)
    }

    @Test
    fun `snapAnimator should use a DecelerateInterpolator with SNAP_ANIMATION_DURATION for top toolbar translations`() {
        val strategy = TopViewBehaviorStrategy()

        assertTrue(strategy.animator.interpolator is DecelerateInterpolator)
        assertEquals(SNAP_ANIMATION_DURATION, strategy.animator.duration)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy should start with isToolbarExpanding = false`() {
        val strategy = BottomViewBehaviorStrategy()

        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `TopToolbarBehaviorStrategy should start with isToolbarExpanding = false`() {
        val strategy = TopViewBehaviorStrategy()

        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapWithAnimation should collapse toolbar if more than half hidden`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(50f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(51f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(100f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(333f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        verify(strategy, times(4)).collapseWithAnimation(view)
        verify(strategy, never()).expandWithAnimation(view)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapWithAnimation should expand toolbar if more than half visible`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(49f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(0f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(-50f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        verify(strategy, times(3)).expandWithAnimation(view)
        verify(strategy, never()).collapseWithAnimation(view)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapWithAnimation should collapse toolbar if more than half hidden`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-51f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(-100f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(-333f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        verify(strategy, times(3)).collapseWithAnimation(view)
        verify(strategy, never()).expandWithAnimation(view)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapWithAnimation should expand toolbar if more than half visible`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-50f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(-49f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(0f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        doReturn(50f).`when`(view).translationY
        strategy.snapWithAnimation(view)

        verify(strategy, times(4)).expandWithAnimation(view)
        verify(strategy, never()).collapseWithAnimation(view)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should end translations animations if in progress`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()

        strategy.snapImmediately(view)

        verify(animator).end()
        verify(view, never()).translationY
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if half translated`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(50f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if more than half`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(55f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if translated off screen`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(555f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated less than half`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(49f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated 0`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(0f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated inside the screen`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-1f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should end translations animations if in progress`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()

        strategy.snapImmediately(view)

        verify(animator).end()
        verify(view, never()).translationY
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate translate to 0 the toolbar if translated less than half`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-49f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated 0`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(0f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated inside the screen`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(1f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if half translated`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-50f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate away the toolbar if more than half translated`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-55f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = -100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - snapImmediately should translate to 0 the toolbar if translated offscreen`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100).`when`(view).height

        doReturn(-111f).`when`(view).translationY
        strategy.snapImmediately(view)
        verify(view).translationY = -100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - expandWithAnimation should translate the toolbar to to y 0`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val view: View = mock()

        strategy.expandWithAnimation(view)

        verify(strategy).animateToTranslationY(view, 0f)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - expandWithAnimation should translate the toolbar to to y 0`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val view: View = mock()

        strategy.expandWithAnimation(view)

        verify(strategy).animateToTranslationY(view, 0f)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should expand toolbar`() {
        // Setting the scenario in which forceExpandWithAnimation will actually do what the name says.
        // Below this test we can change each variable one at a time to test them in isolation.

        val strategy = spy(BottomViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator).cancel()
        verify(strategy).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not force expand the toolbar if not currently collapsing`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        strategy.wasLastExpanding = true
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not expand if user swipes down`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, 100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - forceExpandWithAnimation should not expand the toolbar if it is already expanded`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(0f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should expand toolbar`() {
        // Setting the scenario in which forceExpandWithAnimation will actually do what the name says.
        // Below this test we can change each variable one at a time to test them in isolation.

        val strategy = spy(TopViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(-100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator).cancel()
        verify(strategy).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not force expand the toolbar if not currently collapsing`() {
        val strategy = spy(TopViewBehaviorStrategy())
        strategy.wasLastExpanding = true
        val animator: ValueAnimator = mock()
        doReturn(true).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(-100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not expand if user swipes up`() {
        val strategy = spy(TopViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(-100f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, 10f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `TopToolbarBehaviorStrategy - forceExpandWithAnimation should not expand the toolbar if it is already expanded`() {
        val strategy = spy(TopViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val animator: ValueAnimator = mock()
        doReturn(false).`when`(animator).isStarted
        strategy.animator = animator
        val view: View = mock()
        doReturn(0f).`when`(view).translationY

        strategy.forceExpandWithAnimation(view, -100f)

        verify(strategy.animator, never()).cancel()
        verify(strategy, never()).expandWithAnimation(any())
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - collapseWithAnimation should animate translating the toolbar down, off-screen`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        strategy.collapseWithAnimation(view)

        verify(strategy).animateToTranslationY(view, 100f)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - collapseWithAnimation should animate translating the toolbar up, off-screen`() {
        val strategy = spy(TopViewBehaviorStrategy())
        val view: View = mock()
        doReturn(100).`when`(view).height

        strategy.collapseWithAnimation(view)

        verify(strategy).animateToTranslationY(view, -100f)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should translate up the toolbar with the distance parameter`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(50f).`when`(view).translationY

        strategy.translate(view, -25f)

        verify(view).translationY = 25f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should translate down the toolbar with the distance parameter`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(50f).`when`(view).translationY

        strategy.translate(view, 25f)

        verify(view).translationY = 75f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate up the toolbar if already expanded`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(0f).`when`(view).translationY

        strategy.translate(view, -1f)

        verify(view).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate up the toolbar more than to 0`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(50f).`when`(view).translationY

        strategy.translate(view, -51f)

        verify(view).translationY = 0f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate down the toolbar if already collapsed`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(100f).`when`(view).translationY

        strategy.translate(view, 1f)

        verify(view).translationY = 100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - translate should not translate down the toolbar more than it's height`() {
        val strategy = BottomViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(50f).`when`(view).translationY

        strategy.translate(view, 51f)

        verify(view).translationY = 100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should translate down the toolbar with the distance parameter`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(-50f).`when`(view).translationY

        strategy.translate(view, 25f)

        verify(view).translationY = -75f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should translate up the toolbar with the distance parameter`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(-50f).`when`(view).translationY

        strategy.translate(view, 25f)

        verify(view).translationY = -75f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate down the toolbar if already expanded`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(0f).`when`(view).translationY

        strategy.translate(view, -1f)

        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate down the toolbar more than to 0`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(-50f).`when`(view).translationY

        strategy.translate(view, -51f)

        verify(view).translationY = 0f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate up the toolbar if already collapsed`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(-100f).`when`(view).translationY

        strategy.translate(view, 1f)

        verify(view).translationY = -100f
    }

    @Test
    fun `TopToolbarBehaviorStrategy - translate should not translate up the toolbar more than it's height`() {
        val strategy = TopViewBehaviorStrategy()
        val view: View = mock()
        doReturn(100).`when`(view).height
        doReturn(-50f).`when`(view).translationY

        strategy.translate(view, 51f)

        verify(view).translationY = -100f
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - animateToTranslationY should set wasLastExpanding if expanding`() {
        val strategy = BottomViewBehaviorStrategy()
        strategy.wasLastExpanding = false
        val view: View = mock()
        doReturn(50f).`when`(view).translationY

        strategy.animateToTranslationY(view, 10f)
        assertTrue(strategy.wasLastExpanding)

        strategy.animateToTranslationY(view, 60f)
        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `BottomToolbarBehaviorStrategy - animateToTranslationY should animate to the indicated y translation`() {
        val strategy = spy(BottomViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val view = View(testContext)
        val animator: ValueAnimator = spy(strategy.animator)
        strategy.animator = animator

        strategy.animateToTranslationY(view, 10f)

        verify(animator).start()
        animator.end()
        assertEquals(10f, view.translationY)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - animateToTranslationY should set wasLastExpanding if expanding`() {
        val strategy = TopViewBehaviorStrategy()
        strategy.wasLastExpanding = false
        val view: View = mock()
        doReturn(-50f).`when`(view).translationY

        strategy.animateToTranslationY(view, -10f)
        assertTrue(strategy.wasLastExpanding)

        strategy.animateToTranslationY(view, -60f)
        assertFalse(strategy.wasLastExpanding)
    }

    @Test
    fun `TopToolbarBehaviorStrategy - animateToTranslationY should animate to the indicated y translation`() {
        val strategy = spy(TopViewBehaviorStrategy())
        strategy.wasLastExpanding = false
        val view = View(testContext)
        val animator: ValueAnimator = spy(strategy.animator)
        strategy.animator = animator

        strategy.animateToTranslationY(view, -10f)

        verify(animator).start()
        animator.end()
        assertEquals(-10f, view.translationY)
    }
}
