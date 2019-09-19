/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.intent

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import android.os.Bundle
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_SESSION
import androidx.browser.customtabs.TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor.Companion.ACTION_VIEW_PWA
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class TrustedWebActivityIntentProcessorTest {

    private val apiKey = "XXXXXXXXX"

    @Test
    fun `matches checks if intent is a trusted web activity intent`() {
        val processor = TrustedWebActivityIntentProcessor(mock(), mock(), mock(), mock(), apiKey, mock())

        assertFalse(processor.matches(Intent(ACTION_VIEW_PWA)))
        assertFalse(processor.matches(Intent(ACTION_VIEW)))
        assertFalse(processor.matches(
            Intent(ACTION_VIEW).apply { putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true) }
        ))
        assertFalse(processor.matches(
            Intent(ACTION_VIEW).apply {
                putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                putExtra(EXTRA_SESSION, null as Bundle?)
            }
        ))
        assertTrue(processor.matches(
            Intent(ACTION_VIEW).apply {
                putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
                putExtra(EXTRA_SESSION, null as Bundle?)
            }
        ))
    }

    @Test
    fun `process checks if intent action is not valid`() = runBlockingTest {
        val processor = TrustedWebActivityIntentProcessor(mock(), mock(), mock(), mock(), apiKey, mock())

        assertFalse(processor.process(Intent(ACTION_VIEW_PWA)))
        assertFalse(processor.process(Intent(ACTION_VIEW)))
        assertFalse(processor.process(
            Intent(ACTION_VIEW).apply { putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true) }
        ))
        assertFalse(processor.process(
            Intent(ACTION_VIEW).apply {
                putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                putExtra(EXTRA_SESSION, null as Bundle?)
            }
        ))
        assertFalse(processor.process(
            Intent(ACTION_VIEW).apply {
                putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
                putExtra(EXTRA_SESSION, null as Bundle?)
            }
        ))
        assertFalse(processor.process(
            Intent(ACTION_VIEW, null).apply {
                putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
                putExtra(EXTRA_SESSION, null as Bundle?)
            }
        ))
    }

    @Test
    fun `process adds custom tab config`() = runBlockingTest {
        val intent = Intent(ACTION_VIEW, "https://example.com".toUri()).apply {
            putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
            putExtra(EXTRA_SESSION, null as Bundle?)
        }

        val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase = mock()
        val store: CustomTabsServiceStore = mock()

        val processor = spy(TrustedWebActivityIntentProcessor(mock(), loadUrlUseCase, mock(), mock(), apiKey, store))

        assertTrue(processor.process(intent))

        verify(loadUrlUseCase).invoke(eq("https://example.com"), any(), eq(EngineSession.LoadUrlFlags.external()))
        verify(store, never()).state
    }
}
