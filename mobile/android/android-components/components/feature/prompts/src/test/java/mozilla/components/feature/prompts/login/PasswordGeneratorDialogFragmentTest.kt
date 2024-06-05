/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.MockitoAnnotations.openMocks

@RunWith(AndroidJUnit4::class)
class PasswordGeneratorDialogFragmentTest {

    private lateinit var fragment: PasswordGeneratorDialogFragment

    @Before
    fun setup() {
        openMocks(this)
        fragment = spy(
            PasswordGeneratorDialogFragment.newInstance(
                sessionId = "sessionId",
                promptRequestUID = "uid",
                generatedPassword = "StrongPassword123#",
                currentUrl = "https://www.mozilla.org",
                onSavedGeneratedPassword = {},
                colorsProvider = mock(),
            ),
        )
    }

    @Test
    fun `build dialog`() {
        doReturn(appCompatContext).`when`(fragment).requireContext()
        val dialog = fragment.onCreateDialog(null)
        dialog.show()

        assertEquals(fragment.sessionId, "sessionId")
        assertEquals(fragment.promptRequestUID, "uid")

        assertEquals(dialog.isShowing, true)
    }
}
