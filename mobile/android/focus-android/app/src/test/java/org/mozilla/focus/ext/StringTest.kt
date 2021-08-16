/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StringTest {
    @Test
    fun testBeautifyUrl() {
        assertEqualsBeautified(
            "wikipedia.org/wiki/Adler_Planetarium",
            "https://en.m.wikipedia.org/wiki/Adler_Planetarium"
        )

        assertEqualsBeautified(
            "youtube.com/watch?v=WXqGDW7kuAk",
            "https://youtube.com/watch?v=WXqGDW7kuAk"
        )

        assertEqualsBeautified(
            "spotify.com/album/6JVdzwuTEaLj7Tga8DpFpz",
            "http://open.spotify.com/album/6JVdzwuTEaLj7Tga8DpFpz"
        )

        assertEqualsBeautified(
            "google.com/mail/…/0#inbox/15e34774924ddfb5",
            "https://mail.google.com/mail/u/0/#inbox/15e34774924ddfb5"
        )

        assertEqualsBeautified(
            "google.com/store/…/details?id=com.facebook.katana",
            "https://play.google.com/store/apps/details?id=com.facebook.katana&hl=en"
        )

        assertEqualsBeautified(
            "amazon.com/Mockingjay-Hunger-Games-Suzanne-Collins/…/ref=pd_sim_14_2?_encoding=UTF8",
            "http://amazon.com/Mockingjay-Hunger-Games-Suzanne-Collins/dp/0545663261/ref=pd_sim_14_2?_encoding=UTF8&psc=1&refRID=90ZHE3V976TKBGDR9VAM"
        )

        assertEqualsBeautified(
            "usbank.com/Auth/Login",
            "https://onlinebanking.usbank.com/Auth/Login"
        )

        assertEqualsBeautified(
            "wsj.com/articles/mexican-presidential-candidate-calls-for-nafta-talks-to-be-suspended-1504137175",
            "https://www.wsj.com/articles/mexican-presidential-candidate-calls-for-nafta-talks-to-be-suspended-1504137175"
        )

        assertEqualsBeautified(
            "nytimes.com/2017/…/princess-diana-death-anniversary.html?hp",
            "https://www.nytimes.com/2017/08/30/world/europe/princess-diana-death-anniversary.html?hp&action=click&pgtype=Homepage&clickSource=story-heading&module=second-column-region&region=top-news&WT.nav=top-news"
        )

        assertEqualsBeautified(
            "yahoo.co.jp/hl?a=20170830-00000008-jct-soci",
            "https://headlines.yahoo.co.jp/hl?a=20170830-00000008-jct-soci"
        )

        assertEqualsBeautified(
            "tomshardware.co.uk/answers/…/running-guest-network-channel-interference.html",
            "http://www.tomshardware.co.uk/answers/id-2025922/running-guest-network-channel-interference.html"
        )

        assertEqualsBeautified(
            "github.com/mozilla-mobile/…/1231",
            "https://github.com/mozilla-mobile/focus-android/issues/1231"
        )
    }

    private fun assertEqualsBeautified(expected: String, url: String) {
        assertEquals("beautify($url)", expected, url.beautifyUrl())
    }
}
