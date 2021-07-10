/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.PendingIntent
import android.content.Intent
import android.content.IntentSender
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import io.mockk.every
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.intent.StartSearchIntentProcessor

@RunWith(FenixRobolectricTestRunner::class)
class PrivateShortcutCreateManagerTest {

    @Before
    fun setup() {
        mockkStatic(ShortcutManagerCompat::class)
        mockkStatic(PendingIntent::class)
    }

    @After
    fun tearDown() {
        unmockkStatic(ShortcutManagerCompat::class)
        unmockkStatic(PendingIntent::class)
    }

    @Test
    fun `GIVEN shortcut pinning is not supported WHEN createPrivateShortcut is called THEN do not create a pinned shortcut`() {
        every { ShortcutManagerCompat.isRequestPinShortcutSupported(testContext) } returns false

        PrivateShortcutCreateManager.createPrivateShortcut(testContext)

        verify(exactly = 0) { ShortcutManagerCompat.requestPinShortcut(testContext, any(), any()) }
    }

    @Test
    fun `GIVEN shortcut pinning is supported WHEN createPrivateShortcut is called THEN create a pinned shortcut`() {
        val shortcut = slot<ShortcutInfoCompat>()
        val intentSender = slot<IntentSender>()
        val intent = slot<Intent>()

        every { ShortcutManagerCompat.isRequestPinShortcutSupported(testContext) } returns true

        PrivateShortcutCreateManager.createPrivateShortcut(testContext)

        verify { PendingIntent.getActivity(testContext, 0, capture(intent), PendingIntent.FLAG_UPDATE_CURRENT) }
        verify { ShortcutManagerCompat.requestPinShortcut(testContext, capture(shortcut), capture(intentSender)) }
        `assert shortcutInfoCompat is build correctly`(shortcut.captured)
        `assert homeScreenIntent is built correctly`(intent.captured)
    }

    private fun `assert shortcutInfoCompat is build correctly`(shortcutInfoCompat: ShortcutInfoCompat) {
        assertEquals(testContext.getString(R.string.app_name_private_5, testContext.getString(R.string.app_name)), shortcutInfoCompat.shortLabel)
        assertEquals(testContext.getString(R.string.app_name_private_5, testContext.getString(R.string.app_name)), shortcutInfoCompat.longLabel)
        assertEquals(R.mipmap.ic_launcher_private_round, shortcutInfoCompat.icon.resId)
        `assert homeActivity intent is built correctly`(shortcutInfoCompat.intent)
    }

    private fun `assert homeActivity intent is built correctly`(intent: Intent) {
        assertEquals(Intent.ACTION_VIEW, intent.action)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK, intent.flags)
        assertEquals(HomeActivity::class.qualifiedName, intent.component?.className)
        assertEquals(true, intent.extras?.getBoolean(HomeActivity.PRIVATE_BROWSING_MODE))
        assertEquals(StartSearchIntentProcessor.PRIVATE_BROWSING_PINNED_SHORTCUT, intent.extras?.getString(HomeActivity.OPEN_TO_SEARCH))
    }

    private fun `assert homeScreenIntent is built correctly`(intent: Intent) {
        assertEquals(Intent.ACTION_MAIN, intent.action)
        assert(intent.categories.contains(Intent.CATEGORY_HOME))
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.flags)
    }
}
