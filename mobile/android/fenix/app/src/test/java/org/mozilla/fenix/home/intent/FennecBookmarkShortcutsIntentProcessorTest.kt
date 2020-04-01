/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.net.Uri
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isEqualTo
import assertk.assertions.isFalse
import assertk.assertions.isTrue
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import io.mockk.verifyAll
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.feature.session.SessionUseCases
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.home.intent.FennecBookmarkShortcutsIntentProcessor.Companion.ACTION_FENNEC_HOMESCREEN_SHORTCUT
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.util.UUID

@RunWith(FenixRobolectricTestRunner::class)
class FennecBookmarkShortcutsIntentProcessorTest {
    private val sessionManager = mockk<SessionManager>(relaxed = true)
    private val loadUrlUseCase = mockk<SessionUseCases.DefaultLoadUrlUseCase>(relaxed = true)

    @Test
    fun `do not process blank Intents`() = runBlocking {
        val processor = FennecBookmarkShortcutsIntentProcessor(sessionManager, loadUrlUseCase)
        val fennecShortcutsIntent = Intent(ACTION_FENNEC_HOMESCREEN_SHORTCUT)
        fennecShortcutsIntent.data = Uri.parse("http://mozilla.org")

        val wasEmptyIntentProcessed = processor.process(Intent())

        assertThat(wasEmptyIntentProcessed).isFalse()
        verify {
            sessionManager wasNot Called
            loadUrlUseCase wasNot Called
        }
    }

    @Test
    fun `processing a Fennec shortcut Intent results in loading it's URL in a new Session`() = runBlocking {
        mockkStatic(UUID::class)
        // The Session constructor uses UUID.randomUUID().toString() as the default value for it's id field
        every { UUID.randomUUID().toString() } returns "test"
        val processor = FennecBookmarkShortcutsIntentProcessor(sessionManager, loadUrlUseCase)
        val fennecShortcutsIntent = Intent(ACTION_FENNEC_HOMESCREEN_SHORTCUT)
        val testUrl = "http://mozilla.org"
        fennecShortcutsIntent.data = Uri.parse(testUrl)
        val expectedSession = Session(testUrl, private = false, source = Session.Source.HOME_SCREEN)

        val wasIntentProcessed = processor.process(fennecShortcutsIntent)

        assertAll {
            assertThat(wasIntentProcessed).isTrue()
            assertThat(fennecShortcutsIntent.action).isEqualTo(Intent.ACTION_VIEW)
            assertThat(fennecShortcutsIntent.getSessionId()).isEqualTo(expectedSession.id)
        }
        verifyAll {
            sessionManager.add(expectedSession, true)
            loadUrlUseCase(testUrl, expectedSession, EngineSession.LoadUrlFlags.external())
        }
    }
}
