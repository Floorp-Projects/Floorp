/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.view.inputmethod.EditorInfo
import android.widget.EditText
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class BrowserToolbarTest {

    @Test
    fun testDisplayUrl() {
        val url = "http://mozilla.org"
        val editText = spy(EditText(RuntimeEnvironment.application))
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        doReturn(editText).`when`(toolbar).getUrlViewComponent()

        toolbar.displayUrl(url)

        verify(editText).setText(url)
        verify(editText).setSelection(0, url.length)
    }

    @Test
    fun testUrlChangeListener() {
        var actualUrl: String? = null
        val editText = EditText(RuntimeEnvironment.application)
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        doReturn(editText).`when`(toolbar).getUrlViewComponent()

        toolbar.setOnUrlChangeListener { actualUrl = it }
        toolbar.getUrlViewComponent().setText("http://mozilla.org")
        toolbar.getUrlViewComponent().onEditorAction(EditorInfo.IME_ACTION_GO)

        assertEquals("http://mozilla.org", actualUrl)
    }
}
