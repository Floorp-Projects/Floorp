package org.mozilla.focus.utils

import android.app.Application
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.activity.InfoActivity
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class InfoActivityIntentTest {

    @Test
    fun getIntentFor() {
        val context = ApplicationProvider.getApplicationContext() as Application
        val intent = InfoActivity.getIntentFor(context, "https://www.mozilla.org", "Mozilla")
        val extras = intent.extras
        assertEquals("https://www.mozilla.org", extras?.getString("extra_url"))
        assertEquals("Mozilla", extras?.getString("extra_title"))
    }

    @Test
    fun getAboutIntent() {
        val context = ApplicationProvider.getApplicationContext() as Application
        val intent = InfoActivity.getAboutIntent(context)
        val extras = intent.extras
        assertEquals("focus:about", extras?.getString("extra_url"))
        assertEquals(
            context.resources.getString(R.string.menu_about),
            extras?.getString("extra_title")
        )
    }

    @Test
    fun getRightsIntent() {
        val context = ApplicationProvider.getApplicationContext() as Application
        val intent = InfoActivity.getRightsIntent(context)
        val extras = intent.extras
        assertEquals("focus:rights", extras?.getString("extra_url"))
        assertEquals(
            context.resources.getString(R.string.menu_rights),
            extras?.getString("extra_title")
        )
    }
}
