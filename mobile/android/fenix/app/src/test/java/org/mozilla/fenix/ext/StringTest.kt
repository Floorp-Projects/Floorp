/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

const val PUNYCODE = "xn--kpry57d"
const val IDN = "台灣"

@RunWith(FenixRobolectricTestRunner::class)
class StringTest {

    private val publicSuffixList = PublicSuffixList(testContext)

    @Test
    fun `Simplified Url`() {
        val urlTest = "https://www.amazon.com"
        val new = urlTest.simplifiedUrl()
        assertEquals(new, "amazon.com")
    }

    @Test
    fun `when the full hostname cannot be displayed, elide labels starting from the front`() {
        // See https://url.spec.whatwg.org/#url-rendering-elision
        // See https://chromium.googlesource.com/chromium/src/+/master/docs/security/url_display_guidelines/url_display_guidelines.md#eliding-urls

        val display = "http://1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20.21.22.23.24.25.com"
            .shortened()

        val split = display.split(".")

        // If the list ends with 25.com...
        assertEquals("25", split.dropLast(1).last())
        // ...and each value is 1 larger than the last...
        split.dropLast(1)
            .map { it.toInt() }
            .windowed(2, 1)
            .forEach { (prev, next) ->
                assertEquals(next, prev + 1)
            }
        // ...that means that all removed values came from the front of the list
    }

    @Test
    fun `the registrable domain is always displayed`() {
        // https://url.spec.whatwg.org/#url-rendering-elision
        // See https://chromium.googlesource.com/chromium/src/+/master/docs/security/url_display_guidelines/url_display_guidelines.md#eliding-urls

        val bigRegistrableDomain = "evil-but-also-shockingly-long-registrable-domain.com"
        assertTrue("https://login.your-bank.com.$bigRegistrableDomain/enter/your/password".shortened().contains(bigRegistrableDomain))
    }

    @Test
    fun `url username and password fields should not be displayed`() {
        // See https://url.spec.whatwg.org/#url-rendering-simplification
        // See https://chromium.googlesource.com/chromium/src/+/master/docs/security/url_display_guidelines/url_display_guidelines.md#simplify

        assertFalse("https://examplecorp.com@attacker.example/".shortened().contains("examplecorp"))
        assertFalse("https://examplecorp.com@attacker.example/".shortened().contains("com"))
        assertFalse("https://user:password@example.com/".shortened().contains("user"))
        assertFalse("https://user:password@example.com/".shortened().contains("password"))
    }

    @Test
    fun `eTLDs should not be dropped`() {
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1554984#c11
        "http://mozfreddyb.github.io/" shortenedShouldBecome "mozfreddyb.github.io"
        "http://web.security.plumbing/" shortenedShouldBecome "web.security.plumbing"
    }

    @Test
    fun `ipv4 addresses should be returned as is`() {
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1554984#c11
        "192.168.85.1" shortenedShouldBecome "192.168.85.1"
    }

    @Test
    fun `about buildconfig should not be modified`() {
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1554984#c11
        "about:buildconfig" shortenedShouldBecome "about:buildconfig"
    }

    @Test
    fun `encoded userinfo should still be considered userinfo`() {
        "https://user:password%40really.evil.domain%2F@mail.google.com" shortenedShouldBecome "mail.google.com"
    }

    @Test
    @Ignore("This would be more correct, but does not appear to be an attack vector")
    fun `should decode DWORD IP addresses`() {
        "https://16843009" shortenedShouldBecome "1.1.1.1"
    }

    @Test
    @Ignore("This would be more correct, but does not appear to be an attack vector")
    fun `should decode octal IP addresses`() {
        "https://000010.000010.000010.000010" shortenedShouldBecome "8.8.8.8"
    }

    @Test
    @Ignore("This would be more correct, but does not appear to be an attack vector")
    fun `should decode hex IP addresses`() {
        "http://0x01010101" shortenedShouldBecome "1.1.1.1"
    }

    // BEGIN test cases borrowed from desktop (shortUrl is used for Top Sites on new tab)
    // Test cases are modified, as we show the eTLD
    // (https://searchfox.org/mozilla-central/source/browser/components/newtab/test/unit/lib/ShortUrl.test.js)
    @Test
    fun `should return a blank string if url is blank`() {
        "" shortenedShouldBecome ""
    }

    @Test
    fun `should return the 'url' if not a valid url`() {
        "something" shortenedShouldBecome "something"
        "http:" shortenedShouldBecome "http:"
        "http::double-colon" shortenedShouldBecome "http::double-colon"
        // The largest allowed port is 65,535
        "http://badport:65536/" shortenedShouldBecome "http://badport:65536/"
    }

    @Test
    fun `should convert host to idn when calling shortURL`() {
        "http://$PUNYCODE.blah.com" shortenedShouldBecome "$IDN.blah.com"
    }

    @Test
    fun `should get the hostname from url`() {
        "http://bar.com" shortenedShouldBecome "bar.com"
    }

    @Test
    fun `should not strip out www if not first subdomain`() {
        "http://foo.www.com" shortenedShouldBecome "foo.www.com"
        "http://www.foo.www.com" shortenedShouldBecome "foo.www.com"
    }

    @Test
    fun `should convert to lowercase`() {
        "HTTP://FOO.COM" shortenedShouldBecome "foo.com"
    }

    @Test
    fun `should not include the port`() {
        "http://foo.com:8888" shortenedShouldBecome "foo.com"
    }

    @Test
    fun `should return hostname for localhost`() {
        "http://localhost:8000/" shortenedShouldBecome "localhost"
    }

    @Test
    fun `should return hostname for ip address`() {
        "http://127.0.0.1/foo" shortenedShouldBecome "127.0.0.1"
    }

    @Test
    fun `should return etld for www gov uk (www-only non-etld)`() {
        "https://www.gov.uk/countersigning" shortenedShouldBecome "gov.uk"
    }

    @Test
    fun `should return idn etld for www-only non-etld`() {
        "https://www.$PUNYCODE/foo" shortenedShouldBecome IDN
    }

    @Test
    fun `file uri should return input`() {
        "file:///foo/bar.txt" shortenedShouldBecome "file:///foo/bar.txt"
    }

    @Test
    @Ignore("This behavior conflicts with https://bugzilla.mozilla.org/show_bug.cgi?id=1554984#c11")
    fun `should return not the protocol for about`() {
        "about:newtab" shortenedShouldBecome "newtab"
    }

    @Test
    fun `should fall back to full url as a last resort`() {
        "about:" shortenedShouldBecome "about:"
    }
    // END test cases borrowed from desktop

    // BEGIN test cases borrowed from FFTV
    // (https://searchfox.org/mozilla-mobile/source/firefox-echo-show/app/src/test/java/org/mozilla/focus/utils/TestFormattedDomain.java#228)
    @Test
    fun testIsIPv4RealAddress() {
        assertTrue("192.168.1.1".isIpv4())
        assertTrue("8.8.8.8".isIpv4())
        assertTrue("63.245.215.20".isIpv4())
    }

    @Test
    fun testIsIPv4WithProtocol() {
        assertFalse("http://8.8.8.8".isIpv4())
        assertFalse("https://8.8.8.8".isIpv4())
    }

    @Test
    fun testIsIPv4WithPort() {
        assertFalse("8.8.8.8:400".isIpv4())
        assertFalse("8.8.8.8:1337".isIpv4())
    }

    @Test
    fun testIsIPv4WithPath() {
        assertFalse("8.8.8.8/index.html".isIpv4())
        assertFalse("8.8.8.8/".isIpv4())
    }

    @Test
    fun testIsIPv4WithIPv6() {
        assertFalse("2001:db8::1 ".isIpv4())
        assertFalse("2001:db8:0:1:1:1:1:1".isIpv4())
        assertFalse("[2001:db8:a0b:12f0::1]".isIpv4())
    }
    // END test cases borrowed from FFTV

    private infix fun String.shortenedShouldBecome(expect: String) {
        assertEquals(expect, this.shortened())
    }

    private fun String.shortened() = this.toShortUrl(publicSuffixList)
}
