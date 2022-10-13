/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.share

import android.content.ActivityNotFoundException
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DefaultShareDelegateTest {

    private lateinit var shareDelegate: ShareDelegate

    @Before
    fun setup() {
        shareDelegate = DefaultShareDelegate()
    }

    @Test
    fun `calls onSuccess after starting share chooser`() {
        val context = spy(testContext)
        doNothing().`when`(context).startActivity(any())

        var dismissed = false
        var succeeded = false

        shareDelegate.showShareSheet(
            context,
            ShareData(title = "Title", text = "Text", url = null),
            onDismiss = { dismissed = true },
            onSuccess = { succeeded = true },
        )

        verify(context).startActivity(any())
        assertFalse(dismissed)
        assertTrue(succeeded)
    }

    @Test
    fun `calls onDismiss after share chooser throws error`() {
        val context = spy(testContext)
        doThrow(ActivityNotFoundException()).`when`(context).startActivity(any())

        var dismissed = false
        var succeeded = false

        shareDelegate.showShareSheet(
            context,
            ShareData(title = null, text = "Text", url = "https://example.com"),
            onDismiss = { dismissed = true },
            onSuccess = { succeeded = true },
        )

        assertTrue(dismissed)
        assertFalse(succeeded)
    }
}
