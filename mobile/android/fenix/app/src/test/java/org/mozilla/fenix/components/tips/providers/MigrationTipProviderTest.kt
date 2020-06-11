/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.tips.providers

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_VIEW
import androidx.core.net.toUri
import io.mockk.MockKMatcherScope
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.metrics.MozillaProductDetector.MozillaProducts
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class MigrationTipProviderTest {

    private lateinit var context: Context
    private lateinit var settings: Settings

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        mockkObject(MozillaProductDetector)
        context = spyk(testContext)
        settings = mockk()

        every { context.settings() } returns settings
        every { context.startActivity(any()) } just Runs
        every { settings.shouldDisplayFenixMovingTip() } returns true
    }

    @After
    fun after() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
        unmockkObject(MozillaProductDetector)
    }

    @Test
    fun `test FENIX tip`() {
        every { context.packageName } returns MozillaProducts.FENIX.productName
        val provider = MigrationTipProvider(context)

        val tip = provider.tip!!
        val button = tip.type as TipType.Button
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_button_2),
            button.text
        )
        assertEquals(
            context.getString(R.string.pref_key_migrating_from_fenix_tip),
            tip.identifier
        )
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_header),
            tip.title
        )

        button.action()
        verify { context.startActivity(intentFilterEq(Intent(ACTION_VIEW, SupportUtils.FIREFOX_BETA_PLAY_STORE_URL.toUri()))) }
    }

    @Test
    fun `test FIREFOX_NIGHTLY fenix installed tip`() {
        val launchIntent = mockk<Intent>()
        every { context.packageName } returns MozillaProducts.FIREFOX_NIGHTLY.productName
        every { context.packageManager.getLaunchIntentForPackage(MozillaProducts.FENIX.productName) } returns launchIntent
        every { MozillaProductDetector.packageIsInstalled(context, MozillaProducts.FENIX.productName) } returns true
        val provider = MigrationTipProvider(context)

        val tip = provider.tip!!
        val button = tip.type as TipType.Button
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_button_preview_installed),
            button.text
        )
        assertEquals(
            context.getString(R.string.pref_key_migrating_from_firefox_nightly_tip),
            tip.identifier
        )
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_header_preview_installed),
            tip.title
        )

        button.action()
        verify { context.startActivity(launchIntent) }
    }

    @Test
    fun `test FIREFOX_NIGHTLY fenix not installed tip`() {
        every { context.packageName } returns MozillaProducts.FIREFOX_NIGHTLY.productName
        every { MozillaProductDetector.packageIsInstalled(context, MozillaProducts.FENIX.productName) } returns false
        val provider = MigrationTipProvider(context)

        val tip = provider.tip!!
        val button = tip.type as TipType.Button
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_button_preview_not_installed),
            button.text
        )
        assertEquals(
            context.getString(R.string.pref_key_migrating_from_firefox_nightly_tip),
            tip.identifier
        )
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_header_preview_not_installed),
            tip.title
        )

        button.action()
        verify { context.startActivity(intentFilterEq(Intent(ACTION_VIEW, SupportUtils.FIREFOX_NIGHTLY_PLAY_STORE_URL.toUri()))) }
    }

    @Test
    fun `test FENIX_NIGHTLY fenix installed tip`() {
        val launchIntent = mockk<Intent>()
        every { context.packageName } returns MozillaProducts.FENIX_NIGHTLY.productName
        every { context.packageManager.getLaunchIntentForPackage(MozillaProducts.FENIX.productName) } returns launchIntent
        every { MozillaProductDetector.packageIsInstalled(context, MozillaProducts.FENIX.productName) } returns true
        val provider = MigrationTipProvider(context)

        val tip = provider.tip!!
        val button = tip.type as TipType.Button
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_button_preview_installed),
            button.text
        )
        assertEquals(
            context.getString(R.string.pref_key_migrating_from_fenix_nightly_tip),
            tip.identifier
        )
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_header_preview_installed),
            tip.title
        )

        button.action()
        verify { context.startActivity(launchIntent) }
    }

    @Test
    fun `test FENIX_NIGHTLY fenix not installed tip`() {
        every { context.packageName } returns MozillaProducts.FENIX_NIGHTLY.productName
        every { MozillaProductDetector.packageIsInstalled(context, MozillaProducts.FENIX.productName) } returns false
        val provider = MigrationTipProvider(context)

        val tip = provider.tip!!
        val button = tip.type as TipType.Button
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_button_preview_not_installed),
            button.text
        )
        assertEquals(
            context.getString(R.string.pref_key_migrating_from_fenix_nightly_tip),
            tip.identifier
        )
        assertEquals(
            context.getString(R.string.tip_firefox_preview_moved_header_preview_not_installed),
            tip.title
        )

        button.action()
        verify { context.startActivity(intentFilterEq(Intent(ACTION_VIEW, SupportUtils.FIREFOX_NIGHTLY_PLAY_STORE_URL.toUri()))) }
    }

    @Test
    fun `test other tip`() {
        every { context.packageName } returns ""
        val provider = MigrationTipProvider(context)

        assertNull(provider.tip)
    }

    @Test
    fun `test shouldDisplay`() {
        every { settings.shouldDisplayFenixMovingTip() } returns false
        assertFalse(MigrationTipProvider(context).shouldDisplay)

        every { settings.shouldDisplayFenixMovingTip() } returns true
        assertTrue(MigrationTipProvider(context).shouldDisplay)
    }

    private fun MockKMatcherScope.intentFilterEq(value: Intent): Intent =
        match { it.filterEquals(value) }
}
