/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.lang.RuntimeException

@RunWith(RobolectricTestRunner::class)
class ReaderViewFeatureTest {

    @Before
    fun setup() {
        ReaderViewFeature.installed = false
    }

    @Test
    fun `start installs webextension`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val engine = mock(Engine::class.java)
        val sessionManager = mock(SessionManager::class.java)

        val readerViewFeature = ReaderViewFeature(context, engine, sessionManager)
        assertFalse(ReaderViewFeature.installed)
        readerViewFeature.start()

        val onSuccess = argumentCaptor<((WebExtension) -> Unit)>()
        val onError = argumentCaptor<((WebExtension, Throwable) -> Unit)>()
        verify(engine, times(1)).installWebExtension(eq(ReaderViewFeature.readerViewWebExt), onSuccess.capture(), onError.capture())
        onSuccess.value.invoke(ReaderViewFeature.readerViewWebExt)
        assertTrue(ReaderViewFeature.installed)

        readerViewFeature.start()
        verify(engine, times(1)).installWebExtension(eq(ReaderViewFeature.readerViewWebExt), onSuccess.capture(), onError.capture())

        onError.value.invoke(ReaderViewFeature.readerViewWebExt, RuntimeException())
    }

    @Test
    fun `start registers observer for selected session`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val engine = mock(Engine::class.java)
        val sessionManager: SessionManager = mock()

        val readerViewFeature = spy(ReaderViewFeature(context, engine, sessionManager))
        readerViewFeature.start()

        verify(readerViewFeature).observeSelected()
    }
}
