/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.graphics.Bitmap
import android.graphics.drawable.BitmapDrawable
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.android.material.textfield.TextInputEditText
import junit.framework.TestCase
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.feature.prompts.R
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.appCompatContext
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class SaveLoginDialogFragmentTest : TestCase() {
    @Test
    fun `dialog should always set the website icon if it is available`() {
        val sessionId = "sessionId"
        val requestUID = "uid"
        val shouldDismissOnLoad = true
        val hint = 42
        val loginUsername = "username"
        val loginPassword = "password"
        val entry: LoginEntry = mock() // valid image to be used as favicon
        `when`(entry.username).thenReturn(loginUsername)
        `when`(entry.password).thenReturn(loginPassword)
        val icon: Bitmap = mock()
        val fragment = spy(
            SaveLoginDialogFragment.newInstance(
                sessionId,
                requestUID,
                shouldDismissOnLoad,
                hint,
                entry,
                icon,
            ),
        )
        doReturn(appCompatContext).`when`(fragment).requireContext()
        doAnswer {
            FrameLayout(appCompatContext).apply {
                addView(TextInputEditText(appCompatContext).apply { id = R.id.username_field })
                addView(TextInputEditText(appCompatContext).apply { id = R.id.password_field })
                addView(ImageView(appCompatContext).apply { id = R.id.host_icon })
            }
        }.`when`(fragment).inflateRootView(any())

        val fragmentView = fragment.onCreateView(mock(), mock(), mock())

        verify(fragment).inflateRootView(any())
        verify(fragment).setupRootView(any())
        assertEquals(sessionId, fragment.sessionId)
        assertEquals(requestUID, fragment.promptRequestUID)
        // Using assertTrue since assertEquals / assertSame would fail here
        assertTrue(loginUsername == fragmentView.findViewById<TextInputEditText>(R.id.username_field).text.toString())
        assertTrue(loginPassword == fragmentView.findViewById<TextInputEditText>(R.id.password_field).text.toString())

        // Actually verifying that the provided image is used
        verify(fragment, times(0)).setImageViewTint(any())
        assertSame(icon, (fragmentView.findViewById<ImageView>(R.id.host_icon).drawable as BitmapDrawable).bitmap)
    }

    @Test
    fun `dialog should use a default tinted icon if favicon is not available`() {
        val sessionId = "sessionId"
        val requestUID = "uid"
        val shouldDismissOnLoad = false
        val hint = 42
        val loginUsername = "username"
        val loginPassword = "password"
        val entry: LoginEntry = mock()
        `when`(entry.username).thenReturn(loginUsername)
        `when`(entry.password).thenReturn(loginPassword)
        val icon: Bitmap? = null // null favicon
        val fragment = spy(
            SaveLoginDialogFragment.newInstance(
                sessionId,
                requestUID,
                shouldDismissOnLoad,
                hint,
                entry,
                icon,
            ),
        )
        val defaultIconResource = R.drawable.mozac_ic_globe
        doReturn(appCompatContext).`when`(fragment).requireContext()
        doAnswer {
            FrameLayout(appCompatContext).apply {
                addView(TextInputEditText(appCompatContext).apply { id = R.id.username_field })
                addView(TextInputEditText(appCompatContext).apply { id = R.id.password_field })
                addView(
                    ImageView(appCompatContext).apply {
                        id = R.id.host_icon
                        setImageResource(defaultIconResource)
                    },
                )
            }
        }.`when`(fragment).inflateRootView(any())

        val fragmentView = fragment.onCreateView(mock(), mock(), mock())

        verify(fragment).inflateRootView(any())
        verify(fragment).setupRootView(any())
        assertEquals(sessionId, fragment.sessionId)
        // Using assertTrue since assertEquals / assertSame would fail here
        assertTrue(loginUsername == fragmentView.findViewById<TextInputEditText>(R.id.username_field).text.toString())
        assertTrue(loginPassword == fragmentView.findViewById<TextInputEditText>(R.id.password_field).text.toString())

        // Actually verifying that the tinted default image is used
        val iconView = fragmentView.findViewById<ImageView>(R.id.host_icon)
        verify(fragment).setImageViewTint(iconView)
        assertNotNull(iconView.imageTintList)
        // The icon sent was null, we want the default instead
        assertNotNull(iconView.drawable)
        assertEquals(defaultIconResource, Shadows.shadowOf(iconView.drawable).createdFromResId)
    }
}
