/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.app.Activity
import android.content.Context
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.children
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.StartDownloadDialogLayoutBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class StartDownloadDialogTest {
    @Test
    fun `WHEN the view is to be shown THEN set the scrim and other window customization bind the download values`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val dialogParent = FrameLayout(testContext)
        val dialogContainer = FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.layoutParams = CoordinatorLayout.LayoutParams(0, 0)
        }
        val dialog = TestDownloadDialog(activity)

        mockkStatic("mozilla.components.support.ktx.android.view.WindowKt", "org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true)
            val fluentDialog = dialog.show(dialogContainer)

            val scrim = dialogParent.children.first { it.id == R.id.scrim }
            assertTrue(scrim.hasOnClickListeners())
            assertFalse(scrim.isSoundEffectsEnabled)
            assertTrue(dialog.wasDownloadDataBinded)
            assertEquals(
                Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL,
                (dialogContainer.layoutParams as CoordinatorLayout.LayoutParams).gravity,
            )
            assertEquals(
                testContext.resources.getDimension(R.dimen.browser_fragment_download_dialog_elevation),
                dialogContainer.elevation,
            )
            assertTrue(dialogContainer.isVisible)
            assertEquals(dialog, fluentDialog)
        }
    }

    @Test
    fun `GIVEN a dismiss callback WHEN the dialog is dismissed THEN the callback is informed`() {
        var wasDismissCalled = false
        val dialog = TestDownloadDialog(mockk(relaxed = true))

        val fluentDialog = dialog.onDismiss { wasDismissCalled = true }
        dialog.onDismiss()

        assertTrue(wasDismissCalled)
        assertEquals(dialog, fluentDialog)
    }

    @Test
    fun `GIVEN the download dialog is shown WHEN dismissed THEN remove the scrim, the dialog and any window customizations`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val dialogParent = FrameLayout(testContext)
        val dialogContainer = FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.layoutParams = CoordinatorLayout.LayoutParams(0, 0)
        }
        val dialog = TestDownloadDialog(activity)
        mockkStatic("mozilla.components.support.ktx.android.view.WindowKt", "org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true)
            dialog.show(dialogContainer)
            dialog.binding = StartDownloadDialogLayoutBinding
                .inflate(LayoutInflater.from(activity), dialogContainer, true)

            dialog.dismiss()

            assertNull(dialogParent.children.firstOrNull { it.id == R.id.scrim })
            assertTrue(dialogParent.childCount == 1)
            assertTrue(dialogContainer.childCount == 0)
            assertFalse(dialogContainer.isVisible)
        }
    }

    @Test
    fun `GIVEN a ViewGroup WHEN enabling accessibility THEN enable it for all children but the dialog container`() {
        val activity: Activity = mockk(relaxed = true)
        val dialogParent = FrameLayout(testContext)
        FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.id = R.id.startDownloadDialogContainer
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
        }
        val otherView = View(testContext).also {
            dialogParent.addView(it)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
        }
        val dialog = TestDownloadDialog(activity)

        dialog.enableSiblingsAccessibility(dialogParent)

        assertEquals(listOf(otherView), dialogParent.children.filter { it.isImportantForAccessibility }.toList())
    }

    @Test
    fun `GIVEN a ViewGroup WHEN disabling accessibility THEN disable it for all children but the dialog container`() {
        val activity: Activity = mockk(relaxed = true)
        val dialogParent = FrameLayout(testContext)
        val dialogContainer = FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.id = R.id.startDownloadDialogContainer
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        View(testContext).also {
            dialogParent.addView(it)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        val dialog = TestDownloadDialog(activity)

        dialog.disableSiblingsAccessibility(dialogParent)

        assertEquals(listOf(dialogContainer), dialogParent.children.filter { it.isImportantForAccessibility }.toList())
    }

    @Test
    fun `GIVEN accessibility services are enabled WHEN the dialog is shown THEN disable siblings accessibility`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val dialogParent = FrameLayout(testContext)
        val dialogContainer = FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.id = R.id.startDownloadDialogContainer
            it.layoutParams = CoordinatorLayout.LayoutParams(0, 0)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        View(testContext).also {
            dialogParent.addView(it)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }

        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            val dialog = TestDownloadDialog(activity)

            val settings: Settings = mockk {
                every { accessibilityServicesEnabled } returns false
            }
            every { any<Context>().settings() } returns settings
            dialog.show(dialogContainer)
            assertEquals(2, dialogParent.children.count { it.isImportantForAccessibility })

            every { settings.accessibilityServicesEnabled } returns true
            dialog.show(dialogContainer)
            assertEquals(listOf(dialogContainer), dialogParent.children.filter { it.isImportantForAccessibility }.toList())
        }
    }

    @Test
    fun `WHEN the dialog is dismissed THEN re-enable siblings accessibility`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val dialogParent = FrameLayout(testContext)
        val dialogContainer = FrameLayout(testContext).also {
            dialogParent.addView(it)
            it.id = R.id.startDownloadDialogContainer
            it.layoutParams = CoordinatorLayout.LayoutParams(0, 0)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        val accessibleView = View(testContext).also {
            dialogParent.addView(it)
            it.importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            val settings: Settings = mockk {
                every { accessibilityServicesEnabled } returns true
            }
            every { any<Context>().settings() } returns settings
            val dialog = TestDownloadDialog(activity)
            dialog.show(dialogContainer)
            dialog.binding = StartDownloadDialogLayoutBinding
                .inflate(LayoutInflater.from(activity), dialogContainer, true)

            dialog.dismiss()

            assertEquals(
                listOf(accessibleView),
                dialogParent.children.filter { it.isVisible && it.isImportantForAccessibility }.toList(),
            )
        }
    }
}

private class TestDownloadDialog(
    activity: Activity,
) : StartDownloadDialog(activity) {
    var wasDownloadDataBinded = false

    override fun setupView() {
        wasDownloadDataBinded = true
    }
}
