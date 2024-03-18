/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.view.View
import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class LoginSelectBarTest {
    val login =
        Login(guid = "A", origin = "https://www.mozilla.org", username = "username", password = "password")
    val login2 =
        Login(guid = "B", origin = "https://www.mozilla.org", username = "username2", password = "password")

    @Test
    fun `showPicker updates visibility`() {
        val bar = LoginSelectBar(appCompatContext)

        bar.showPrompt(listOf(login, login2))

        assertTrue(bar.isVisible)
    }

    @Test
    fun `hidePicker updates visibility`() {
        val bar = spy(LoginSelectBar(appCompatContext))

        bar.hidePrompt()

        verify(bar).visibility = View.GONE
    }

    @Test
    fun `listener is invoked when clicking manage logins option`() {
        val bar = LoginSelectBar(appCompatContext)
        val listener: SelectablePromptView.Listener<Login> = mock()

        assertNull(bar.listener)

        bar.listener = listener
        bar.showPrompt(listOf(login, login2))

        bar.findViewById<AppCompatTextView>(R.id.manage_logins).performClick()

        verify(listener).onManageOptions()
    }

    @Test
    fun `listener is invoked when clicking a login option`() {
        val bar = LoginSelectBar(appCompatContext)
        val listener: SelectablePromptView.Listener<Login> = mock()

        assertNull(bar.listener)

        bar.listener = listener
        bar.showPrompt(listOf(login, login2))

        val adapter = bar.findViewById<RecyclerView>(R.id.logins_list).adapter as BasicLoginAdapter
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        holder.itemView.performClick()

        verify(listener).onOptionSelect(login)
    }

    @Test
    fun `view is expanded when clicking header`() {
        val bar = LoginSelectBar(appCompatContext)

        bar.showPrompt(listOf(login, login2))

        bar.findViewById<AppCompatTextView>(R.id.saved_logins_header).performClick()

        // Expanded
        assertTrue(bar.findViewById<RecyclerView>(R.id.logins_list).isVisible)
        assertTrue(bar.findViewById<AppCompatTextView>(R.id.manage_logins).isVisible)

        bar.findViewById<AppCompatTextView>(R.id.saved_logins_header).performClick()

        // Hidden
        assertFalse(bar.findViewById<RecyclerView>(R.id.logins_list).isVisible)
        assertFalse(bar.findViewById<AppCompatTextView>(R.id.manage_logins).isVisible)
    }
}
