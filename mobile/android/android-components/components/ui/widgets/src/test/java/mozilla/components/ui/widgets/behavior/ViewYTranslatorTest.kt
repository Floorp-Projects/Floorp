/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ViewYTranslatorTest {
    @Test
    fun `yTranslator should use BottomToolbarBehaviorStrategy for bottom placed toolbars`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)

        assertTrue(yTranslator.strategy is BottomViewBehaviorStrategy)
    }

    @Test
    fun `yTranslator should use TopToolbarBehaviorStrategy for top placed toolbars`() {
        val yTranslator = ViewYTranslator(ViewPosition.TOP)

        assertTrue(yTranslator.strategy is TopViewBehaviorStrategy)
    }

    @Test
    fun `yTranslator should delegate it's strategy for snapWithAnimation`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.snapWithAnimation(view)

        verify(strategy).snapWithAnimation(view)
    }

    @Test
    fun `yTranslator should delegate it's strategy for expandWithAnimation`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.expandWithAnimation(view)

        verify(strategy).expandWithAnimation(view)
    }

    @Test
    fun `yTranslator should delegate it's strategy for collapseWithAnimation`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.collapseWithAnimation(view)

        verify(strategy).collapseWithAnimation(view)
    }

    @Test
    fun `yTranslator should delegate it's strategy for forceExpandIfNotAlready`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.forceExpandIfNotAlready(view, 14f)

        verify(strategy).forceExpandWithAnimation(view, 14f)
    }

    @Test
    fun `yTranslator should delegate it's strategy for translate`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.translate(view, 23f)

        verify(strategy).translate(view, 23f)
    }

    @Test
    fun `yTranslator should delegate it's strategy for cancelInProgressTranslation`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy

        yTranslator.cancelInProgressTranslation()

        verify(strategy).cancelInProgressTranslation()
    }

    @Test
    fun `yTranslator should delegate it's strategy for snapImmediately`() {
        val yTranslator = ViewYTranslator(ViewPosition.BOTTOM)
        val strategy: ViewYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val view: View = mock()

        yTranslator.snapImmediately(view)

        verify(strategy).snapImmediately(view)
    }
}
