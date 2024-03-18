/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.Locale

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
        val versionName = "testVersion"

        val testTopic = SupportUtils.SumoTopic.TRACKERS
        val testTopicStr = testTopic.topicStr

        Locale.setDefault(Locale.GERMANY)
        assertEquals(
            "https://support.mozilla.org/1/mobile/$versionName/Android/de-DE/$testTopicStr",
            SupportUtils.getSumoURLForTopic(versionName, testTopic),
        )

        Locale.setDefault(Locale.CANADA_FRENCH)
        assertEquals(
            "https://support.mozilla.org/1/mobile/$versionName/Android/fr-CA/$testTopicStr",
            SupportUtils.getSumoURLForTopic(versionName, testTopic),
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
            SupportUtils.manifestoURL,
        )

        Locale.setDefault(Locale.KOREA)
        assertEquals(
            "https://www.mozilla.org/ko-KR/about/manifesto/",
            SupportUtils.manifestoURL,
        )
    }
}
