/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Intent
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.support.utils.toSafeIntent
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.components.metrics.Event
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class CustomTabActivityTest {

    @Test
    fun getIntentSource() {
        val activity = CustomTabActivity()

        val launcherIntent = Intent(Intent.ACTION_MAIN).apply {
            addCategory(Intent.CATEGORY_LAUNCHER)
        }.toSafeIntent()
        assertEquals(Event.OpenedApp.Source.CUSTOM_TAB, activity.getIntentSource(launcherIntent))

        val viewIntent = Intent(Intent.ACTION_VIEW).toSafeIntent()
        assertEquals(Event.OpenedApp.Source.CUSTOM_TAB, activity.getIntentSource(viewIntent))

        val otherIntent = Intent().toSafeIntent()
        assertEquals(Event.OpenedApp.Source.CUSTOM_TAB, activity.getIntentSource(otherIntent))
    }
}
