/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ToolbarViewTest {

    @MockK(relaxed = true) private lateinit var interactor: ToolbarInteractor
    @MockK private lateinit var engine: Engine
    private lateinit var context: Context
    private lateinit var toolbar: BrowserToolbar

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        toolbar = spyk(BrowserToolbar(context))
    }

    @Test
    fun `sets up interactor listeners`() {
        val urlCommitListener = slot<(String) -> Boolean>()
        val editListener = slot<Toolbar.OnEditListener>()
        every { toolbar.setOnUrlCommitListener(capture(urlCommitListener)) } just Runs
        every { toolbar.setOnEditListener(capture(editListener)) } just Runs

        buildToolbarView(isPrivate = false)

        assertFalse(urlCommitListener.captured("test"))
        verify { interactor.onUrlCommitted("test") }

        assertFalse(editListener.captured.onCancelEditing())
        verify { interactor.onEditingCanceled() }

        editListener.captured.onTextChanged("https://example.com")
        verify { interactor.onTextChanged("https://example.com") }
    }

    @Test
    fun `sets toolbar to normal mode`() {
        buildToolbarView(isPrivate = false)
        assertFalse(toolbar.private)
    }

    @Test
    fun `sets toolbar to private mode`() {
        buildToolbarView(isPrivate = true)
        assertTrue(toolbar.private)
    }

    private fun buildToolbarView(isPrivate: Boolean) = ToolbarView(
        context,
        interactor,
        historyStorage = null,
        isPrivate = isPrivate,
        view = toolbar,
        engine = engine
    )
}
