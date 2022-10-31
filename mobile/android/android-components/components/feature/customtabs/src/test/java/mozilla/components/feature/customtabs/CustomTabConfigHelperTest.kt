/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.content.Intent
import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Binder
import android.os.Bundle
import androidx.browser.customtabs.CustomTabColorSchemeParams
import androidx.browser.customtabs.CustomTabsIntent
import androidx.browser.customtabs.CustomTabsIntent.EXTRA_NAVIGATION_BAR_COLOR
import androidx.browser.customtabs.TrustedWebUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class CustomTabConfigHelperTest {

    private lateinit var resources: Resources

    @Before
    fun setup() {
        resources = spy(testContext.resources)
        doReturn(24f).`when`(resources).getDimension(R.dimen.mozac_feature_customtabs_max_close_button_size)
    }

    @Test
    fun isCustomTabIntent() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        assertTrue(isCustomTabIntent(customTabsIntent.intent))
        assertFalse(isCustomTabIntent(mock<Intent>()))
    }

    @Test
    fun isTrustedWebActivityIntent() {
        val customTabsIntent = CustomTabsIntent.Builder().build().intent
        val trustedWebActivityIntent = Intent(customTabsIntent)
            .putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
        assertTrue(isTrustedWebActivityIntent(trustedWebActivityIntent))
        assertFalse(isTrustedWebActivityIntent(customTabsIntent))
        assertFalse(isTrustedWebActivityIntent(mock<Intent>()))
        assertFalse(
            isTrustedWebActivityIntent(
                Intent().putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true),
            ),
        )
    }

    @Test
    fun createFromIntentWithToolbarColor() {
        val builder = CustomTabsIntent.Builder()
        val customTabColorSchemeBuilder = CustomTabColorSchemeParams.Builder()
        customTabColorSchemeBuilder.setToolbarColor(Color.BLACK)
        builder.setDefaultColorSchemeParams(customTabColorSchemeBuilder.build())

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertEquals(Color.BLACK, customTabConfig.toolbarColor)
    }

    @Test
    fun createFromIntentWithNavigationBarColor() {
        val intent = CustomTabsIntent.Builder().build().intent.apply {
            putExtra(EXTRA_NAVIGATION_BAR_COLOR, Color.WHITE)
        }

        val customTabConfig = createCustomTabConfigFromIntent(intent, testContext.resources)
        assertEquals(Color.WHITE, customTabConfig.navigationBarColor)
    }

    @Test
    fun createFromIntentWithCloseButton() {
        val size = 24
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
        assertEquals(size, customTabConfig.closeButtonIcon?.width)
        assertEquals(size, customTabConfig.closeButtonIcon?.height)

        val customTabConfigNoResources = createCustomTabConfigFromIntent(builder.build().intent, null)
        assertEquals(closeButtonIcon, customTabConfigNoResources.closeButtonIcon)
        assertEquals(size, customTabConfigNoResources.closeButtonIcon?.width)
        assertEquals(size, customTabConfigNoResources.closeButtonIcon?.height)
    }

    @Test
    fun createFromIntentWithMaxOversizedCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertNull(customTabConfig.closeButtonIcon)

        val customTabConfigNoResources = createCustomTabConfigFromIntent(builder.build().intent, null)
        assertEquals(closeButtonIcon, customTabConfigNoResources.closeButtonIcon)
    }

    @Test
    fun createFromIntentUsingDisplayMetricsForCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val resources: Resources = mock()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        `when`(resources.getDimension(R.dimen.mozac_feature_customtabs_max_close_button_size)).thenReturn(64f)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, resources)
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
    }

    @Test
    fun createFromIntentWithInvalidCloseButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        // Intent is a parcelable but not a Bitmap
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, Intent())

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertNull(customTabConfig.closeButtonIcon)
    }

    @Test
    fun createFromIntentWithUrlbarHiding() {
        val builder = CustomTabsIntent.Builder()
        builder.setUrlBarHidingEnabled(true)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertTrue(customTabConfig.enableUrlbarHiding)
    }

    @Test
    fun createFromIntentWithShareMenuItem() {
        val builder = CustomTabsIntent.Builder()
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertTrue(customTabConfig.showShareMenuItem)
    }

    @Test
    fun createFromIntentWithShareState() {
        val builder = CustomTabsIntent.Builder()
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON)

        val extraShareState = builder.build().intent.getIntExtra(CustomTabsIntent.EXTRA_SHARE_STATE, 5)
        assertEquals(CustomTabsIntent.SHARE_STATE_ON, extraShareState)
    }

    @Test
    fun createFromIntentWithCustomizedMenu() {
        val builder = CustomTabsIntent.Builder()
        val pendingIntent = PendingIntent.getActivity(null, 0, null, 0)
        builder.addMenuItem("menuitem1", pendingIntent)
        builder.addMenuItem("menuitem2", pendingIntent)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertEquals(2, customTabConfig.menuItems.size)
        assertEquals("menuitem1", customTabConfig.menuItems[0].name)
        assertSame(pendingIntent, customTabConfig.menuItems[0].pendingIntent)
        assertEquals("menuitem2", customTabConfig.menuItems[1].name)
        assertSame(pendingIntent, customTabConfig.menuItems[1].pendingIntent)
    }

    @Test
    fun createFromIntentWithActionButton() {
        val builder = CustomTabsIntent.Builder()

        val bitmap = mock<Bitmap>()
        val intent = PendingIntent.getActivity(testContext, 0, Intent("testAction"), 0)
        builder.setActionButton(bitmap, "desc", intent)

        val customTabsIntent = builder.build()
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertNotNull(customTabConfig.actionButtonConfig)
        assertEquals("desc", customTabConfig.actionButtonConfig?.description)
        assertEquals(intent, customTabConfig.actionButtonConfig?.pendingIntent)
        assertEquals(bitmap, customTabConfig.actionButtonConfig?.icon)
        assertFalse(customTabConfig.actionButtonConfig!!.tint)
    }

    @Test
    fun createFromIntentWithInvalidActionButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val invalid = Bundle()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, invalid)
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithInvalidExtras() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val extrasField = Intent::class.java.getDeclaredField("mExtras")
        extrasField.isAccessible = true
        extrasField.set(customTabsIntent.intent, null)
        extrasField.isAccessible = false

        assertFalse(isCustomTabIntent(customTabsIntent.intent))

        // Make sure we're not failing
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertNotNull(customTabConfig)
        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithExitAnimationOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        val bundle = Bundle()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE, bundle)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertEquals(bundle, customTabConfig.exitAnimations)
    }

    @Test
    fun createFromIntentWithPageTitleOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertTrue(customTabConfig.titleVisible)
    }

    @Test
    fun createFromIntentWithSessionToken() {
        val customTabsIntent: Intent = mock()
        val bundle: Bundle = mock()
        val binder: Binder = mock()
        `when`(customTabsIntent.extras).thenReturn(bundle)
        `when`(bundle.getBinder(CustomTabsIntent.EXTRA_SESSION)).thenReturn(binder)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent, testContext.resources)
        assertNotNull(customTabConfig.sessionToken)
    }
}
