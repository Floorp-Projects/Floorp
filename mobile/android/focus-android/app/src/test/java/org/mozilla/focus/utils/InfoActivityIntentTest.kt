package org.mozilla.focus.utils

import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

import org.junit.Assert.assertEquals
import org.mozilla.focus.R
import org.mozilla.focus.activity.InfoActivity

@RunWith(RobolectricTestRunner::class)
class InfoActivityIntentTest {

    @Test
    fun getIntentFor() {
        val context = RuntimeEnvironment.application
        val intent = InfoActivity.getIntentFor(context, "https://www.mozilla.org", "Mozilla")
        val extras = intent.extras
        assertEquals("https://www.mozilla.org", extras?.getString("extra_url"))
        assertEquals("Mozilla", extras?.getString("extra_title"))
    }

    @Test
    fun getAboutIntent() {
        val context = RuntimeEnvironment.application
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
        val context = RuntimeEnvironment.application
        val intent = InfoActivity.getRightsIntent(context)
        val extras = intent.extras
        assertEquals("focus:rights", extras?.getString("extra_url"))
        assertEquals(
            context.resources.getString(R.string.menu_rights),
            extras?.getString("extra_title")
        )
    }
}
