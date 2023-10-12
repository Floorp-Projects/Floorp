/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.mozilla.components.feature.addons.ui

import android.graphics.Bitmap
import androidx.appcompat.widget.AppCompatImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonDialogFragment
import mozilla.components.feature.addons.ui.KEY_ICON
import mozilla.components.feature.addons.ui.setIcon
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import mozilla.components.support.utils.ext.getParcelableCompat
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AddonDialogFragmentTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `loadIcon the add-on icon successfully`() {
        val addon = mock<Addon>()
        val bitmap = mock<Bitmap>()
        val iconView = mock<AppCompatImageView>()
        val fragment = createAddonDialogFragment()

        fragment.safeArguments.putParcelable(KEY_ICON, bitmap)

        fragment.loadIcon(addon, iconView)

        verify(iconView).setImageDrawable(Mockito.any())
    }

    @Test
    fun `loadIcon the add-on icon with a null result`() {
        val addon = mock<Addon>()
        val bitmap = mock<Bitmap>()
        val iconView = mock<AppCompatImageView>()
        val fragment = createAddonDialogFragment()

        whenever(addon.provideIcon()).thenReturn(bitmap)

        assertNull(fragment.arguments?.getParcelableCompat(KEY_ICON, Bitmap::class.java))

        fragment.loadIcon(addon, iconView)

        assertNotNull(fragment.arguments?.getParcelableCompat(KEY_ICON, Bitmap::class.java))
        verify(iconView).setIcon(addon)
    }

    private fun createAddonDialogFragment(): AddonDialogFragment {
        val dialog = AddonDialogFragment()
        return spy(dialog).apply {
            doNothing().`when`(this).dismiss()
        }
    }
}
