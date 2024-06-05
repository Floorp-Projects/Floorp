/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.view.View
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
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
    fun `show prompt updates visibility`() {
        val bar = SuggestStrongPasswordBar(appCompatContext)
        val listener: PasswordPromptView.Listener = mock()
        Assert.assertNull(bar.listener)
        bar.listener = listener
        Assert.assertNotNull(bar.listener)

        bar.showPrompt()
        Assert.assertTrue(bar.isVisible)
    }
}
