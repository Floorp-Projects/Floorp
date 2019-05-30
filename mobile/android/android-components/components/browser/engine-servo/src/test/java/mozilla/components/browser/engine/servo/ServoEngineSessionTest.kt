/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.engine.servo

import android.net.Uri
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.servoview.ServoView
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ServoEngineSessionTest {

    @Test
    fun `loadUrl() with absent view not crashed`() {
        // Given
        val session = ServoEngineSession()

        // When
        session.loadUrl("http://mozilla.org")

        // Then
        assertTrue("Not crashed", true)
    }

    @Test
    fun `loadUrl() with predefined view loads url`() {
        // Given
        val view = mock<ServoView>()
        val engineView = mock<ServoEngineView>().apply {
            whenever(this.servoView).then { view }
        }
        val session = ServoEngineSession().apply {
            attachView(engineView)
        }
        val uri = Uri.parse("http://mozilla.org")

        // When
        session.loadUrl(uri.toString())

        // Then
        verify(view).loadUri(uri)
    }

    @Test
    fun `loadUrl() with unset view loads url later`() {
        // Given
        val session = ServoEngineSession()
        val uri = Uri.parse("http://mozilla.org")

        // When
        session.loadUrl(uri.toString())

        // Then
        assertTrue("Not crashed", true)

        // When
        val view = mock<ServoView>()
        val engineView = mock<ServoEngineView>().apply {
            whenever(this.servoView).then { view }
        }
        session.attachView(engineView)

        // Then
        verify(view).loadUri(uri)
    }

    @Test
    fun `stopLoading() forwarded to view`() {
        // Given
        val view = mock<ServoView>()
        val session = ServoEngineSession().apply {
            val engineView = mock<ServoEngineView>().apply {
                whenever(this.servoView).then { view }
            }
            attachView(engineView)
        }

        // When
        session.stopLoading()

        // Then
        verify(view).stop()
    }

    @Test
    fun `reload() forwarded to view`() {
        // Given
        val view = mock<ServoView>()
        val session = ServoEngineSession().apply {
            val engineView = mock<ServoEngineView>().apply {
                whenever(this.servoView).then { view }
            }
            attachView(engineView)
        }

        // When
        session.reload()

        // Then
        verify(view).reload()
    }

    @Test
    fun `goBack() forwarded to view`() {
        // Given
        val view = mock<ServoView>()
        val session = ServoEngineSession().apply {
            val engineView = mock<ServoEngineView>().apply {
                whenever(this.servoView).then { view }
            }
            attachView(engineView)
        }

        // When
        session.goBack()

        // Then
        verify(view).goBack()
    }

    @Test
    fun `goForward() forwarded to view`() {
        // Given
        val view = mock<ServoView>()
        val session = ServoEngineSession().apply {
            val engineView = mock<ServoEngineView>().apply {
                whenever(this.servoView).then { view }
            }
            attachView(engineView)
        }

        // When
        session.goForward()

        // Then
        verify(view).goForward()
    }

    @Test
    fun `saveState() returns emptyState`() {
        // Given
        val session = ServoEngineSession()

        // When
        val state = session.saveState()

        // Then
        assertEquals("Saved state is empty",
            ServoEngineSessionState(), state)
    }
}