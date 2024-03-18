/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.view.View
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.concept.PasswordPromptView
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito

@RunWith(AndroidJUnit4::class)
class SuggestStrongPasswordBarTest {

    @Test
    fun `hide prompt updates visibility`() {
        val bar = Mockito.spy(SuggestStrongPasswordBar(appCompatContext))
        bar.hidePrompt()
        Mockito.verify(bar).visibility = View.GONE
    }

    @Test
    fun `listener is invoked when clicking use strong password option`() {
        val bar = SuggestStrongPasswordBar(appCompatContext)
        val listener: PasswordPromptView.Listener = mock()
        val suggestedPassword = "generatedPassword123#"
        val url = "https://wwww.abc.com"
        val onSaveLoginWithGeneratedPass: (String, String) -> Unit = mock()
        Assert.assertNull(bar.listener)
        bar.listener = listener
        bar.showPrompt(suggestedPassword, url, onSaveLoginWithGeneratedPass)
        bar.findViewById<AppCompatTextView>(R.id.use_strong_password).performClick()
        Mockito.verify(listener)
            .onUseGeneratedPassword(suggestedPassword, url, onSaveLoginWithGeneratedPass)
    }

    @Test
    fun `view is expanded when clicking header`() {
        val bar = SuggestStrongPasswordBar(appCompatContext)
        val suggestedPassword = "generatedPassword123#"

        bar.showPrompt(suggestedPassword, "") { _, _ -> }

        bar.findViewById<AppCompatTextView>(R.id.suggest_strong_password_header).performClick()
        // Expanded
        Assert.assertTrue(bar.findViewById<RecyclerView>(R.id.use_strong_password).isVisible)

        bar.findViewById<AppCompatTextView>(R.id.suggest_strong_password_header).performClick()
        // Hidden
        Assert.assertFalse(bar.findViewById<RecyclerView>(R.id.use_strong_password).isVisible)
    }
}
