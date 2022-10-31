/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserToolbarYTranslatorTest {
    @Test
    fun `yTranslator should use BottomToolbarBehaviorStrategy for bottom placed toolbars`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)

        assertTrue(yTranslator.strategy is BottomToolbarBehaviorStrategy)
    }

    @Test
    fun `yTranslator should use TopToolbarBehaviorStrategy for top placed toolbars`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.TOP)

        assertTrue(yTranslator.strategy is TopToolbarBehaviorStrategy)
    }

    @Test
    fun `yTranslator should delegate it's strategy for snapWithAnimation`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.snapWithAnimation(toolbar)

        verify(strategy).snapWithAnimation(toolbar)
    }

    @Test
    fun `yTranslator should delegate it's strategy for expandWithAnimation`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.expandWithAnimation(toolbar)

        verify(strategy).expandWithAnimation(toolbar)
    }

    @Test
    fun `yTranslator should delegate it's strategy for collapseWithAnimation`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.collapseWithAnimation(toolbar)

        verify(strategy).collapseWithAnimation(toolbar)
    }

    @Test
    fun `yTranslator should delegate it's strategy for forceExpandIfNotAlready`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.forceExpandIfNotAlready(toolbar, 14f)

        verify(strategy).forceExpandWithAnimation(toolbar, 14f)
    }

    @Test
    fun `yTranslator should delegate it's strategy for translate`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.translate(toolbar, 23f)

        verify(strategy).translate(toolbar, 23f)
    }

    @Test
    fun `yTranslator should delegate it's strategy for cancelInProgressTranslation`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy

        yTranslator.cancelInProgressTranslation()

        verify(strategy).cancelInProgressTranslation()
    }

    @Test
    fun `yTranslator should delegate it's strategy for snapImmediately`() {
        val yTranslator = BrowserToolbarYTranslator(ToolbarPosition.BOTTOM)
        val strategy: BrowserToolbarYTranslationStrategy = mock()
        yTranslator.strategy = strategy
        val toolbar: BrowserToolbar = mock()

        yTranslator.snapImmediately(toolbar)

        verify(strategy).snapImmediately(toolbar)
    }
}
