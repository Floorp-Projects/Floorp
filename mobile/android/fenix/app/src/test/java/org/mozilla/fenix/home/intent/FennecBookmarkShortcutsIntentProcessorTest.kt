/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.net.Uri
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.intent.FennecBookmarkShortcutsIntentProcessor.Companion.ACTION_FENNEC_HOMESCREEN_SHORTCUT

@RunWith(FenixRobolectricTestRunner::class)
class FennecBookmarkShortcutsIntentProcessorTest {
    private val addNewTabUseCase = mockk<TabsUseCases.AddNewTabUseCase>(relaxed = true)

    @Test
    fun `do not process blank Intents`() = runTest {
        val processor = FennecBookmarkShortcutsIntentProcessor(addNewTabUseCase)
        val fennecShortcutsIntent = Intent(ACTION_FENNEC_HOMESCREEN_SHORTCUT)
        fennecShortcutsIntent.data = Uri.parse("http://mozilla.org")

        val wasEmptyIntentProcessed = processor.process(Intent())

        assertFalse(wasEmptyIntentProcessed)
        verify {
            addNewTabUseCase wasNot Called
        }
    }

    @Test
    fun `processing a Fennec shortcut Intent results in loading it's URL in a new Session`() = runTest {
        val expectedSessionId = "test"
        val processor = FennecBookmarkShortcutsIntentProcessor(addNewTabUseCase)
        val fennecShortcutsIntent = Intent(ACTION_FENNEC_HOMESCREEN_SHORTCUT)
        val testUrl = "http://mozilla.org"
        fennecShortcutsIntent.data = Uri.parse(testUrl)

        every {
            addNewTabUseCase(
                url = testUrl,
                flags = EngineSession.LoadUrlFlags.external(),
                source = SessionState.Source.Internal.HomeScreen,
                selectTab = true,
                startLoading = true,
            )
        } returns expectedSessionId

        val wasIntentProcessed = processor.process(fennecShortcutsIntent)
        assertTrue(wasIntentProcessed)
        assertEquals(Intent.ACTION_VIEW, fennecShortcutsIntent.action)
        assertEquals(expectedSessionId, fennecShortcutsIntent.getSessionId())
        verify {
            addNewTabUseCase(
                url = testUrl,
                flags = EngineSession.LoadUrlFlags.external(),
                source = SessionState.Source.Internal.HomeScreen,
                selectTab = true,
                startLoading = true,
            )
        }
    }
}
