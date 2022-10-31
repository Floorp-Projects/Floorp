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
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor.Companion.ACTION_VIEW_PWA
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
@Suppress("DEPRECATION")
@Ignore("TrustedWebActivityIntentProcessorTest] is deprecated. See https://github.com/mozilla-mobile/android-components/issues/12024")
class TrustedWebActivityIntentProcessorTest {

    private lateinit var store: BrowserStore

    @Before
    fun setup() {
        store = BrowserStore()
    }

    @Test
    fun `process checks if intent action is not valid`() {
        val processor = TrustedWebActivityIntentProcessor(mock(), mock(), mock(), mock())

        assertFalse(processor.process(Intent(ACTION_VIEW_PWA)))
        assertFalse(processor.process(Intent(ACTION_VIEW)))
        assertFalse(
            processor.process(
                Intent(ACTION_VIEW).apply { putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true) },
            ),
        )
        assertFalse(
            processor.process(
                Intent(ACTION_VIEW).apply {
                    putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                    putExtra(EXTRA_SESSION, null as Bundle?)
                },
            ),
        )
        assertFalse(
            processor.process(
                Intent(ACTION_VIEW).apply {
                    putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
                    putExtra(EXTRA_SESSION, null as Bundle?)
                },
            ),
        )
        assertFalse(
            processor.process(
                Intent(ACTION_VIEW, null).apply {
                    putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
                    putExtra(EXTRA_SESSION, null as Bundle?)
                },
            ),
        )
    }

    @Test
    fun `process adds custom tab config`() {
        val intent = Intent(ACTION_VIEW, "https://example.com".toUri()).apply {
            putExtra(EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
            putExtra(EXTRA_SESSION, null as Bundle?)
        }

        val customTabsStore: CustomTabsServiceStore = mock()
        val addTabUseCase: CustomTabsUseCases.AddCustomTabUseCase = mock()

        val processor = TrustedWebActivityIntentProcessor(addTabUseCase, mock(), mock(), customTabsStore)
        assertTrue(processor.process(intent))

        verify(addTabUseCase).invoke(
            "https://example.com",
            source = SessionState.Source.Internal.HomeScreen,
            customTabConfig = CustomTabConfig(externalAppType = ExternalAppType.TRUSTED_WEB_ACTIVITY),
        )
    }
}
