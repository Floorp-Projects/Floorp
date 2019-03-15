/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.tab

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Bundle
import android.support.customtabs.CustomTabsIntent
import android.util.DisplayMetrics
import androidx.test.core.app.ApplicationProvider
import mozilla.components.support.test.mock
import mozilla.components.support.utils.SafeIntent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CustomTabConfigTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun isCustomTabIntent() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        assertTrue(CustomTabConfig.isCustomTabIntent(SafeIntent(customTabsIntent.intent)))
        assertFalse(CustomTabConfig.isCustomTabIntent(SafeIntent(mock(Intent::class.java))))
    }

    @Test
    fun createFromIntentAssignsId() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertTrue(customTabConfig.id.isNotBlank())
    }

    @Test
    fun createFromIntentWithToolbarColor() {
        val builder = CustomTabsIntent.Builder()
        builder.setToolbarColor(Color.BLACK)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertEquals(Color.BLACK, customTabConfig.toolbarColor)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.TOOLBAR_COLOR_OPTION))
    }

    @Test
    fun createFromIntentWithCloseButton() {
        val size = 24
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
        assertEquals(size, customTabConfig.closeButtonIcon?.width)
        assertEquals(size, customTabConfig.closeButtonIcon?.height)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.CLOSE_BUTTON_OPTION))
    }

    @Test
    fun createFromIntentWithMaxOversizedCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertNull(customTabConfig.closeButtonIcon)
        assertFalse(customTabConfig.options.contains(CustomTabConfig.CLOSE_BUTTON_OPTION))
    }

    @Test
    fun createFromIntentUsingDisplayMetricsForCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val displayMetrics: DisplayMetrics = mock()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        displayMetrics.density = 3f

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)), displayMetrics)
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.CLOSE_BUTTON_OPTION))
    }

    @Test
    fun createFromIntentWithInvalidCloseButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        // Intent is a parcelable but not a Bitmap
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, Intent())

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertNull(customTabConfig.closeButtonIcon)
        assertFalse(customTabConfig.options.contains(CustomTabConfig.CLOSE_BUTTON_OPTION))
    }

    @Test
    fun createFromIntentWithUrlbarHiding() {
        val builder = CustomTabsIntent.Builder()
        builder.enableUrlBarHiding()

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertFalse(customTabConfig.disableUrlbarHiding)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.DISABLE_URLBAR_HIDING_OPTION))
    }

    @Test
    fun createFromIntentWithShareMenuItem() {
        val builder = CustomTabsIntent.Builder()
        builder.addDefaultShareMenuItem()

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertTrue(customTabConfig.showShareMenuItem)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.SHARE_MENU_ITEM_OPTION))
    }

    @Test
    fun createFromIntentWithCustomizedMenu() {
        val builder = CustomTabsIntent.Builder()
        val pendingIntent = PendingIntent.getActivity(null, 0, null, 0)
        builder.addMenuItem("menuitem1", pendingIntent)
        builder.addMenuItem("menuitem2", pendingIntent)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((builder.build().intent)))
        assertEquals(2, customTabConfig.menuItems.size)
        assertEquals("menuitem1", customTabConfig.menuItems[0].name)
        assertSame(pendingIntent, customTabConfig.menuItems[0].pendingIntent)
        assertEquals("menuitem2", customTabConfig.menuItems[1].name)
        assertSame(pendingIntent, customTabConfig.menuItems[1].pendingIntent)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.CUSTOMIZED_MENU_OPTION))
    }

    @Test
    fun createFromIntentWithActionButton() {
        val builder = CustomTabsIntent.Builder()

        val bitmap = mock(Bitmap::class.java)
        val intent = PendingIntent.getActivity(context, 0, Intent("testAction"), 0)
        builder.setActionButton(bitmap, "desc", intent)

        val customTabsIntent = builder.build()
        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent(customTabsIntent.intent))

        assertNotNull(customTabConfig.actionButtonConfig)
        assertEquals("desc", customTabConfig.actionButtonConfig?.description)
        assertEquals(intent, customTabConfig.actionButtonConfig?.pendingIntent)
        assertEquals(bitmap, customTabConfig.actionButtonConfig?.icon)
        assertTrue(customTabConfig.options.contains(CustomTabConfig.ACTION_BUTTON_OPTION))
    }

    @Test
    fun createFromIntentWithInvalidActionButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val invalid = Bundle()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, invalid)
        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent(customTabsIntent.intent))

        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithInvalidExtras() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val extrasField = Intent::class.java.getDeclaredField("mExtras")
        extrasField.isAccessible = true
        extrasField.set(customTabsIntent.intent, null)
        extrasField.isAccessible = false

        assertFalse(CustomTabConfig.isCustomTabIntent(SafeIntent(customTabsIntent.intent)))

        // Make sure we're not failing
        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent(customTabsIntent.intent))
        assertNotNull(customTabConfig)
        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithActionButtonTint() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON, true)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertTrue(customTabConfig.options.contains(CustomTabConfig.ACTION_BUTTON_TINT_OPTION))
    }

    @Test
    fun createFromIntentWithBottomToolbarOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, Bundle())

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertTrue(customTabConfig.options.contains(CustomTabConfig.BOTTOM_TOOLBAR_OPTION))
    }

    @Test
    fun createFromIntentWithExitAnimationOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE, Bundle())

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertTrue(customTabConfig.options.contains(CustomTabConfig.EXIT_ANIMATION_OPTION))
    }

    @Test
    fun createFromIntentWithPageTitleOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE)

        val customTabConfig = CustomTabConfig.createFromIntent(SafeIntent((customTabsIntent.intent)))
        assertTrue(customTabConfig.options.contains(CustomTabConfig.PAGE_TITLE_OPTION))
    }
}
