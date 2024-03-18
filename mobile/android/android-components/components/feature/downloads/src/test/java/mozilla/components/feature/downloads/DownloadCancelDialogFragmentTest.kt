/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.downloads.ui.DownloadCancelDialogFragment
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class DownloadCancelDialogFragmentTest {

    @Test
    fun `WHEN accept button is clicked THEN onAcceptClicked must be called`() {
        spy(DownloadCancelDialogFragment.newInstance(2)).apply {
            doReturn(testContext).`when`(this).requireContext()
            doReturn(mockFragmentManager()).`when`(this).parentFragmentManager
            var wasAcceptClicked = false
            onAcceptClicked = { _, _ -> wasAcceptClicked = true }

            with(onCreateDialog(null)) {
                findViewById<Button>(R.id.accept_button).apply { performClick() }
            }

            assertTrue(wasAcceptClicked)
        }
    }

    @Test
    fun `WHEN deny button is clicked THEN onDenyClicked must be called`() {
        spy(DownloadCancelDialogFragment.newInstance(2)).apply {
            doReturn(testContext).`when`(this).requireContext()
            doReturn(mockFragmentManager()).`when`(this).parentFragmentManager
            var wasDenyCalled = false
            onDenyClicked = { wasDenyCalled = true }

            with(onCreateDialog(null)) {
                findViewById<Button>(R.id.deny_button).apply { performClick() }
            }

            assertTrue(wasDenyCalled)
        }
    }

    @Test
    fun `WHEN overriding strings are provided to the prompt, THEN they are used by the prompt`() {
        val testText = DownloadCancelDialogFragment.PromptText(
            titleText = R.string.mozac_feature_downloads_cancel_active_private_downloads_warning_content_body,
            bodyText = R.string.mozac_feature_downloads_cancel_active_downloads_warning_content_title,
            acceptText = R.string.mozac_feature_downloads_cancel_active_private_downloads_deny,
            denyText = R.string.mozac_feature_downloads_cancel_active_downloads_accept,
        )
        spy(
            DownloadCancelDialogFragment.newInstance(
                0,
                promptText = testText,
            ),
        ).apply {
            doReturn(testContext).`when`(this).requireContext()

            with(onCreateDialog(null)) {
                findViewById<TextView>(R.id.title).apply {
                    Assert.assertEquals(text, testContext.getString(testText.titleText))
                }
                findViewById<TextView>(R.id.body).apply {
                    Assert.assertEquals(text, testContext.getString(testText.bodyText))
                }
                findViewById<Button>(R.id.accept_button).apply {
                    Assert.assertEquals(text, testContext.getString(testText.acceptText))
                }
                findViewById<Button>(R.id.deny_button).apply {
                    Assert.assertEquals(text, testContext.getString(testText.denyText))
                }
            }
        }
    }

    @Test
    fun `WHEN styling is provided to the prompt, THEN it's used by the prompt`() {
        val testStyling = DownloadCancelDialogFragment.PromptStyling(
            gravity = Gravity.TOP,
            shouldWidthMatchParent = false,
            positiveButtonBackgroundColor = android.R.color.white,
            positiveButtonTextColor = android.R.color.black,
            positiveButtonRadius = 4f,
        )

        spy(
            DownloadCancelDialogFragment.newInstance(
                0,
                promptStyling = testStyling,
            ),
        ).apply {
            doReturn(testContext).`when`(this).requireContext()

            with(onCreateDialog(null)) {
                with(window!!.attributes) {
                    Assert.assertTrue(gravity == Gravity.TOP)
                    Assert.assertTrue(width == ViewGroup.LayoutParams.WRAP_CONTENT)
                }

                with(findViewById<Button>(R.id.accept_button)) {
                    Assert.assertEquals(
                        ContextCompat.getColor(
                            testContext,
                            testStyling.positiveButtonBackgroundColor!!,
                        ),
                        (background as GradientDrawable).color?.defaultColor,
                    )
                    Assert.assertEquals(
                        testStyling.positiveButtonRadius!!,
                        (background as GradientDrawable).cornerRadius,
                    )
                    Assert.assertEquals(
                        ContextCompat.getColor(
                            testContext,
                            testStyling.positiveButtonTextColor!!,
                        ),
                        textColors.defaultColor,
                    )
                }
            }
        }
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()
        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        return fragmentManager
    }
}
