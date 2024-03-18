/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import android.widget.TextView
import androidx.appcompat.widget.AppCompatSpinner
import androidx.core.view.isVisible
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.WebsitePermissionsView.PermissionViewHolder.SpinnerPermission
import org.mozilla.fenix.settings.quicksettings.WebsitePermissionsView.PermissionViewHolder.ToggleablePermission
import java.util.EnumMap

@RunWith(FenixRobolectricTestRunner::class)
class WebsitePermissionViewTest {

    @MockK(relaxed = true)
    private lateinit var interactor: WebsitePermissionInteractor
    private lateinit var view: WebsitePermissionsView

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        view = spyk(WebsitePermissionsView(FrameLayout(testContext), interactor))
    }

    @Test
    fun `update - with visible permissions`() {
        val label = TextView(testContext)
        val status = TextView(testContext)
        val permission = WebsitePermission.Toggleable(
            phoneFeature = PhoneFeature.CAMERA,
            status = "status",
            isVisible = true,
            isEnabled = true,
            isBlockedByAndroid = false,
        )

        val map = mapOf<PhoneFeature, WebsitePermission>(PhoneFeature.CAMERA to permission)

        view.permissionViews = EnumMap(
            mapOf(PhoneFeature.CAMERA to ToggleablePermission(label, status)),
        )

        every { view.bindPermission(any(), any()) } returns Unit

        view.update(map)

        verify { interactor.onPermissionsShown() }
        verify { view.bindPermission(any(), any()) }
    }

    @Test
    fun `update - with none visible permissions`() {
        val label = TextView(testContext)
        val status = TextView(testContext)
        val permission = WebsitePermission.Toggleable(
            phoneFeature = PhoneFeature.CAMERA,
            status = "status",
            isVisible = false,
            isEnabled = true,
            isBlockedByAndroid = false,
        )

        val map = mapOf<PhoneFeature, WebsitePermission>(PhoneFeature.CAMERA to permission)

        view.permissionViews =
            EnumMap(mapOf(PhoneFeature.CAMERA to ToggleablePermission(label, status)))

        every { view.bindPermission(any(), any()) } returns Unit

        view.update(map)

        verify(exactly = 0) { interactor.onPermissionsShown() }
        verify { view.bindPermission(any(), any()) }
    }

    @Test
    fun `bindPermission - a visible ToggleablePermission`() {
        val label = TextView(testContext)
        val status = TextView(testContext)
        val permissionView = ToggleablePermission(label, status)
        val permission = WebsitePermission.Toggleable(
            phoneFeature = PhoneFeature.CAMERA,
            status = "status",
            isVisible = true,
            isEnabled = true,
            isBlockedByAndroid = false,
        )

        view.permissionViews = EnumMap(mapOf(PhoneFeature.CAMERA to permissionView))

        every { interactor.onPermissionToggled(any()) } returns Unit

        view.bindPermission(permission, permissionView)

        assertTrue(permissionView.label.isVisible)
        assertTrue(permissionView.label.isEnabled)
        assertTrue(permissionView.status.isVisible)
        assertEquals(permission.status, permissionView.status.text)

        permissionView.status.performClick()

        verify { interactor.onPermissionToggled(any()) }
    }

    @Test
    fun `bindPermission - a not visible ToggleablePermission`() {
        val label = TextView(testContext)
        val status = TextView(testContext)
        val permissionView = ToggleablePermission(label, status)
        val permission = WebsitePermission.Toggleable(
            phoneFeature = PhoneFeature.CAMERA,
            status = "status",
            isVisible = false,
            isEnabled = false,
            isBlockedByAndroid = false,
        )

        view.permissionViews = EnumMap(mapOf(PhoneFeature.CAMERA to permissionView))

        every { interactor.onPermissionToggled(any()) } returns Unit

        view.bindPermission(permission, permissionView)

        assertFalse(permissionView.label.isVisible)
        assertFalse(permissionView.label.isEnabled)
        assertFalse(permissionView.status.isVisible)
        assertEquals(permission.status, permissionView.status.text)

        permissionView.status.performClick()

        verify { interactor.onPermissionToggled(any()) }
    }

    @Test
    fun `bindPermission - a visible SpinnerPermission`() {
        val label = TextView(testContext)
        val status = AppCompatSpinner(testContext)
        val permissionView = SpinnerPermission(label, status)
        val options = listOf(
            AutoplayValue.BlockAll(
                label = "BlockAll",
                rules = mockk(),
                sitePermission = null,
            ),
            AutoplayValue.AllowAll(
                label = "AllowAll",
                rules = mockk(),
                sitePermission = null,
            ),
            AutoplayValue.BlockAudible(
                label = "BlockAudible",
                rules = mockk(),
                sitePermission = null,
            ),
        )
        val permission = WebsitePermission.Autoplay(
            autoplayValue = options[0],
            options = options,
            isVisible = true,
        )

        view.permissionViews = EnumMap(mapOf(PhoneFeature.AUTOPLAY to permissionView))

        every { interactor.onAutoplayChanged(any()) } returns Unit

        view.bindPermission(permission, permissionView)

        assertTrue(permissionView.label.isVisible)
        assertFalse(permissionView.label.isEnabled)
        assertTrue(permissionView.status.isVisible)
        assertEquals(permission.autoplayValue, permissionView.status.selectedItem)

        permissionView.status.onItemSelectedListener!!.onItemSelected(
            mockk(),
            permissionView.status,
            1,
            0L,
        )

        // Selecting the same item should not trigger a selection event.
        verify(exactly = 0) { interactor.onAutoplayChanged(permissionView.status.selectedItem as AutoplayValue) }

        permissionView.status.setSelection(2)
        permissionView.status.onItemSelectedListener!!.onItemSelected(
            mockk(),
            permissionView.status,
            2,
            0L,
        )

        // Selecting a different item from the selected one should trigger an selection event.
        verify(exactly = 1) { interactor.onAutoplayChanged(permissionView.status.selectedItem as AutoplayValue) }
    }
}
