package org.mozilla.focus.utils

import android.app.Application
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.Locale

@RunWith(RobolectricTestRunner::class)
class SupportUtilsTest {

    @Test
    fun cleanup() {
        // Other tests might get confused by our locale fiddling, so lets go back to the default:
        Locale.setDefault(Locale.ENGLISH)
    }

    /*
     * Super simple sumo URL test - it exists primarily to verify that we're setting the language
     * and page tags correctly. appVersion is null in tests, so we just test that there's a null there,
     * which doesn't seem too useful...
     */
    @Test
    @Throws(Exception::class)
    fun getSumoURLForTopic() {
        val context = ApplicationProvider.getApplicationContext() as Application
        val versionName = context.packageManager.getPackageInfo(context.packageName, 0).versionName

        val testTopic = SupportUtils.SumoTopic.TRACKERS
        val testTopicStr = testTopic.topicStr

        Locale.setDefault(Locale.GERMANY)
        assertEquals(
            "https://support.mozilla.org/1/mobile/$versionName/Android/de-DE/$testTopicStr",
            SupportUtils.getSumoURLForTopic(ApplicationProvider.getApplicationContext(), testTopic)
        )

        Locale.setDefault(Locale.CANADA_FRENCH)
        assertEquals(
            "https://support.mozilla.org/1/mobile/$versionName/Android/fr-CA/$testTopicStr",
            SupportUtils.getSumoURLForTopic(ApplicationProvider.getApplicationContext(), testTopic)
        )
    }

    /**
     * This is a pretty boring tests - it exists primarily to verify that we're actually setting
     * a langtag in the manfiesto URL.
     */
    @Test
    @Throws(Exception::class)
    fun getManifestoURL() {
        Locale.setDefault(Locale.UK)
        assertEquals(
            "https://www.mozilla.org/en-GB/about/manifesto/",
            SupportUtils.manifestoURL
        )

        Locale.setDefault(Locale.KOREA)
        assertEquals(
            "https://www.mozilla.org/ko-KR/about/manifesto/",
            SupportUtils.manifestoURL
        )
    }
}
