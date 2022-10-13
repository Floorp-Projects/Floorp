/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class PublicSuffixListTest {

    private val publicSuffixList
        get() = PublicSuffixList(testContext)

    @Test
    fun `Verify getPublicSuffixPlusOne for known domains`() = runTest {
        assertEquals(
            "mozilla.org",
            publicSuffixList.getPublicSuffixPlusOne("www.mozilla.org").await(),
        )

        assertEquals(
            "google.com",
            publicSuffixList.getPublicSuffixPlusOne("google.com").await(),
        )

        assertEquals(
            "foobar.blogspot.com",
            publicSuffixList.getPublicSuffixPlusOne("foobar.blogspot.com").await(),
        )

        assertEquals(
            "independent.co.uk",
            publicSuffixList.getPublicSuffixPlusOne("independent.co.uk").await(),
        )

        assertEquals(
            "independent.co.uk",
            publicSuffixList.getPublicSuffixPlusOne("www.independent.co.uk").await(),
        )

        assertEquals(
            "biz.com.ua",
            publicSuffixList.getPublicSuffixPlusOne("www.biz.com.ua").await(),
        )

        assertEquals(
            "example.org",
            publicSuffixList.getPublicSuffixPlusOne("example.org").await(),
        )

        assertEquals(
            "example.pvt.k12.ma.us",
            publicSuffixList.getPublicSuffixPlusOne("www.example.pvt.k12.ma.us").await(),
        )

        assertEquals(
            "δπθ.gr",
            publicSuffixList.getPublicSuffixPlusOne("www.ουτοπία.δπθ.gr").await(),
        )
    }

    @Test
    fun `Verify getPublicSuffix for known domains`() = runTest {
        assertEquals(
            "org",
            publicSuffixList.getPublicSuffix("www.mozilla.org").await(),
        )

        assertEquals(
            "com",
            publicSuffixList.getPublicSuffix("google.com").await(),
        )

        assertEquals(
            "blogspot.com",
            publicSuffixList.getPublicSuffix("foobar.blogspot.com").await(),
        )

        assertEquals(
            "co.uk",
            publicSuffixList.getPublicSuffix("independent.co.uk").await(),
        )

        assertEquals(
            "co.uk",
            publicSuffixList.getPublicSuffix("www.independent.co.uk").await(),
        )

        assertEquals(
            "com.ua",
            publicSuffixList.getPublicSuffix("www.biz.com.ua").await(),
        )

        assertEquals(
            "org",
            publicSuffixList.getPublicSuffix("example.org").await(),
        )

        assertEquals(
            "pvt.k12.ma.us",
            publicSuffixList.getPublicSuffix("www.example.pvt.k12.ma.us").await(),
        )

        assertEquals(
            "gr",
            publicSuffixList.getPublicSuffix("www.ουτοπία.δπθ.gr").await(),
        )
    }

    @Test
    fun `Verify stripPublicSuffix for known domains`() = runTest {
        assertEquals(
            "www.mozilla",
            publicSuffixList.stripPublicSuffix("www.mozilla.org").await(),
        )

        assertEquals(
            "google",
            publicSuffixList.stripPublicSuffix("google.com").await(),
        )

        assertEquals(
            "foobar",
            publicSuffixList.stripPublicSuffix("foobar.blogspot.com").await(),
        )

        assertEquals(
            "independent",
            publicSuffixList.stripPublicSuffix("independent.co.uk").await(),
        )

        assertEquals(
            "www.independent",
            publicSuffixList.stripPublicSuffix("www.independent.co.uk").await(),
        )

        assertEquals(
            "www.biz",
            publicSuffixList.stripPublicSuffix("www.biz.com.ua").await(),
        )

        assertEquals(
            "example",
            publicSuffixList.stripPublicSuffix("example.org").await(),
        )

        assertEquals(
            "www.example",
            publicSuffixList.stripPublicSuffix("www.example.pvt.k12.ma.us").await(),
        )

        assertEquals(
            "www.ουτοπία.δπθ",
            publicSuffixList.stripPublicSuffix("www.ουτοπία.δπθ.gr").await(),
        )
    }

    /**
     * Short set of test data from:
     * https://raw.githubusercontent.com/publicsuffix/list/master/tests/test_psl.txt
     */
    @Test
    fun `Verify getPublicSuffixPlusOne against official test data`() = runTest {
        // empty input
        assertNull(publicSuffixList.getPublicSuffixPlusOne("").await())

        // Mixed case.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("COM").await())
        assertEquals(
            "example.COM",
            publicSuffixList.getPublicSuffixPlusOne("example.COM").await(),
        )
        assertEquals(
            "eXample.COM",
            publicSuffixList.getPublicSuffixPlusOne("WwW.eXample.COM").await(),
        )

        // Leading dot.
        // ArrayIndexOutOfBoundsException: assertEquals("", publicSuffixList.getPublicSuffixPlusOne(".example.com").await())

        // TLD with only 1 rule.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("biz").await())
        assertEquals(
            "domain.biz",
            publicSuffixList.getPublicSuffixPlusOne("domain.biz").await(),
        )
        assertEquals(
            "domain.biz",
            publicSuffixList.getPublicSuffixPlusOne("b.domain.biz").await(),
        )
        assertEquals(
            "domain.biz",
            publicSuffixList.getPublicSuffixPlusOne("a.b.domain.biz").await(),
        )

        // TLD with some 2-level rules.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("com").await())
        assertEquals(
            "example.com",
            publicSuffixList.getPublicSuffixPlusOne("example.com").await(),
        )
        assertEquals(
            "example.com",
            publicSuffixList.getPublicSuffixPlusOne("b.example.com").await(),
        )
        assertEquals(
            "example.com",
            publicSuffixList.getPublicSuffixPlusOne("a.b.example.com").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("uk.com").await())
        assertEquals(
            "example.uk.com",
            publicSuffixList.getPublicSuffixPlusOne("example.uk.com").await(),
        )
        assertEquals(
            "example.uk.com",
            publicSuffixList.getPublicSuffixPlusOne("b.example.uk.com").await(),
        )
        assertEquals(
            "example.uk.com",
            publicSuffixList.getPublicSuffixPlusOne("a.b.example.uk.com").await(),
        )
        assertEquals(
            "test.ac",
            publicSuffixList.getPublicSuffixPlusOne("test.ac").await(),
        )

        // TLD with only 1 (wildcard) rule.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("mm").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("c.mm").await())
        assertEquals(
            "b.c.mm",
            publicSuffixList.getPublicSuffixPlusOne("b.c.mm").await(),
        )
        assertEquals(
            "b.c.mm",
            publicSuffixList.getPublicSuffixPlusOne("a.b.c.mm").await(),
        )

        // More complex TLD.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("jp").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("ac.jp").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("kyoto.jp").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("ide.kyoto.jp").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("c.kobe.jp").await())
        assertEquals(
            "test.jp",
            publicSuffixList.getPublicSuffixPlusOne("test.jp").await(),
        )
        assertEquals(
            "test.jp",
            publicSuffixList.getPublicSuffixPlusOne("www.test.jp").await(),
        )
        assertEquals(
            "test.ac.jp",
            publicSuffixList.getPublicSuffixPlusOne("test.ac.jp").await(),
        )
        assertEquals(
            "test.ac.jp",
            publicSuffixList.getPublicSuffixPlusOne("www.test.ac.jp").await(),
        )
        assertEquals(
            "test.kyoto.jp",
            publicSuffixList.getPublicSuffixPlusOne("test.kyoto.jp").await(),
        )
        assertEquals(
            "b.ide.kyoto.jp",
            publicSuffixList.getPublicSuffixPlusOne("b.ide.kyoto.jp").await(),
        )
        assertEquals(
            "b.ide.kyoto.jp",
            publicSuffixList.getPublicSuffixPlusOne("a.b.ide.kyoto.jp").await(),
        )
        assertEquals(
            "b.c.kobe.jp",
            publicSuffixList.getPublicSuffixPlusOne("b.c.kobe.jp").await(),
        )
        assertEquals(
            "b.c.kobe.jp",
            publicSuffixList.getPublicSuffixPlusOne("a.b.c.kobe.jp").await(),
        )
        assertEquals(
            "city.kobe.jp",
            publicSuffixList.getPublicSuffixPlusOne("city.kobe.jp").await(),
        )
        assertEquals(
            "city.kobe.jp",
            publicSuffixList.getPublicSuffixPlusOne("www.city.kobe.jp").await(),
        )

        // TLD with a wildcard rule and exceptions.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("ck").await())
        assertNull(publicSuffixList.getPublicSuffixPlusOne("test.ck").await())
        assertEquals(
            "b.test.ck",
            publicSuffixList.getPublicSuffixPlusOne("b.test.ck").await(),
        )
        assertEquals(
            "b.test.ck",
            publicSuffixList.getPublicSuffixPlusOne("a.b.test.ck").await(),
        )
        assertEquals(
            "www.ck",
            publicSuffixList.getPublicSuffixPlusOne("www.ck").await(),
        )
        assertEquals(
            "www.ck",
            publicSuffixList.getPublicSuffixPlusOne("www.www.ck").await(),
        )

        // US K12.
        assertNull(publicSuffixList.getPublicSuffixPlusOne("us").await())
        assertEquals(
            "test.us",
            publicSuffixList.getPublicSuffixPlusOne("test.us").await(),
        )
        assertEquals(
            "test.us",
            publicSuffixList.getPublicSuffixPlusOne("www.test.us").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("ak.us").await())
        assertEquals(
            "test.ak.us",
            publicSuffixList.getPublicSuffixPlusOne("www.test.ak.us").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("k12.ak.us").await())
        assertEquals(
            "test.k12.ak.us",
            publicSuffixList.getPublicSuffixPlusOne("test.k12.ak.us").await(),
        )
        assertEquals(
            "test.k12.ak.us",
            publicSuffixList.getPublicSuffixPlusOne("www.test.k12.ak.us").await(),
        )

        // IDN labels.
        assertEquals(
            "食狮.com.cn",
            publicSuffixList.getPublicSuffixPlusOne("食狮.com.cn").await(),
        )
        // https://github.com/mozilla-mobile/android-components/issues/1777
        assertEquals(
            "食狮.公司.cn",
            publicSuffixList.getPublicSuffixPlusOne("食狮.公司.cn").await(),
        )
        assertEquals(
            "食狮.公司.cn",
            publicSuffixList.getPublicSuffixPlusOne("www.食狮.公司.cn").await(),
        )
        assertEquals(
            "shishi.公司.cn",
            publicSuffixList.getPublicSuffixPlusOne("shishi.公司.cn").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("公司.cn").await())
        assertEquals(
            "食狮.中国",
            publicSuffixList.getPublicSuffixPlusOne("食狮.中国").await(),
        )
        assertEquals(
            "食狮.中国",
            publicSuffixList.getPublicSuffixPlusOne("www.食狮.中国").await(),
        )
        assertEquals(
            "shishi.中国",
            publicSuffixList.getPublicSuffixPlusOne("shishi.中国").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("中国").await())

        // Same as above, but punycoded.
        assertEquals(
            "xn--85x722f.com.cn",
            publicSuffixList.getPublicSuffixPlusOne("xn--85x722f.com.cn").await(),
        )
        // https://github.com/mozilla-mobile/android-components/issues/1777
        assertEquals(
            "xn--85x722f.xn--55qx5d.cn",
            publicSuffixList.getPublicSuffixPlusOne("xn--85x722f.xn--55qx5d.cn").await(),
        )
        assertEquals(
            "xn--85x722f.xn--55qx5d.cn",
            publicSuffixList.getPublicSuffixPlusOne("www.xn--85x722f.xn--55qx5d.cn").await(),
        )
        assertEquals(
            "shishi.xn--55qx5d.cn",
            publicSuffixList.getPublicSuffixPlusOne("shishi.xn--55qx5d.cn").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("xn--55qx5d.cn").await())
        assertEquals(
            "xn--85x722f.xn--fiqs8s",
            publicSuffixList.getPublicSuffixPlusOne("xn--85x722f.xn--fiqs8s").await(),
        )
        assertEquals(
            "xn--85x722f.xn--fiqs8s",
            publicSuffixList.getPublicSuffixPlusOne("www.xn--85x722f.xn--fiqs8s").await(),
        )
        assertEquals(
            "shishi.xn--fiqs8s",
            publicSuffixList.getPublicSuffixPlusOne("shishi.xn--fiqs8s").await(),
        )
        assertNull(publicSuffixList.getPublicSuffixPlusOne("xn--fiqs8s").await())
    }

    @Test
    fun `Accessing with and without prefetch`() = runTest {
        run {
            val publicSuffixList = PublicSuffixList(testContext)
            assertEquals("org", publicSuffixList.getPublicSuffix("mozilla.org").await())
        }

        run {
            val publicSuffixList = PublicSuffixList(testContext).apply {
                prefetch().await()
            }
            assertEquals("org", publicSuffixList.getPublicSuffix("mozilla.org").await())
        }
    }

    @Test
    fun `Verify isPublicSuffix with known and unknown suffixes`() = runTest {
        assertTrue(publicSuffixList.isPublicSuffix("org").await())
        assertTrue(publicSuffixList.isPublicSuffix("com").await())
        assertTrue(publicSuffixList.isPublicSuffix("us").await())
        assertTrue(publicSuffixList.isPublicSuffix("de").await())
        assertTrue(publicSuffixList.isPublicSuffix("de.com").await())
        assertTrue(publicSuffixList.isPublicSuffix("co.uk").await())
        assertTrue(publicSuffixList.isPublicSuffix("taxi.br").await())
        assertTrue(publicSuffixList.isPublicSuffix("edu.cw").await())
        assertTrue(publicSuffixList.isPublicSuffix("chirurgiens-dentistes.fr").await())
        assertTrue(publicSuffixList.isPublicSuffix("trani-andria-barletta.it").await())
        assertTrue(publicSuffixList.isPublicSuffix("yabuki.fukushima.jp").await())
        assertTrue(publicSuffixList.isPublicSuffix("research.museum").await())
        assertTrue(publicSuffixList.isPublicSuffix("lamborghini").await())
        assertTrue(publicSuffixList.isPublicSuffix("reisen").await())
        assertTrue(publicSuffixList.isPublicSuffix("github.io").await())

        assertFalse(publicSuffixList.isPublicSuffix("").await())
        assertFalse(publicSuffixList.isPublicSuffix("mozilla").await())
        assertFalse(publicSuffixList.isPublicSuffix("mozilla.org").await())
        assertFalse(publicSuffixList.isPublicSuffix("ork").await())
        assertFalse(publicSuffixList.isPublicSuffix("us.com.uk").await())
    }

    /**
     * Test cases inspired by Guava tests:
     * https://github.com/google/guava/blob/master/guava-tests/test/com/google/common/net/InternetDomainNameTest.java
     */
    @Test
    fun `Verify getPublicSuffix can handle obscure and invalid input`() = runTest {
        assertEquals("cOM", publicSuffixList.getPublicSuffix("f-_-o.cOM").await())
        assertEquals("com", publicSuffixList.getPublicSuffix("f11-1.com").await())
        assertNull(publicSuffixList.getPublicSuffix("www").await())
        assertEquals("a23", publicSuffixList.getPublicSuffix("abc.a23").await())
        assertEquals("com", publicSuffixList.getPublicSuffix("a\u0394b.com").await())
        assertNull(publicSuffixList.getPublicSuffix("").await())
        assertNull(publicSuffixList.getPublicSuffix(" ").await())
        assertNull(publicSuffixList.getPublicSuffix(".").await())
        assertNull(publicSuffixList.getPublicSuffix("..").await())
        assertNull(publicSuffixList.getPublicSuffix("...").await())
        assertNull(publicSuffixList.getPublicSuffix("woo.com.").await())
        assertNull(publicSuffixList.getPublicSuffix("::1").await())
        assertNull(publicSuffixList.getPublicSuffix("13").await())

        // The following input returns an empty string which does not seem correct:
        // https://github.com/mozilla-mobile/android-components/issues/3541
        assertEquals("", publicSuffixList.getPublicSuffix("foo.net.us\uFF61ocm").await())

        // Technically that may be correct; but it doesn't make sense to return part of an IP as public suffix:
        // https://github.com/mozilla-mobile/android-components/issues/3540
        assertEquals("1", publicSuffixList.getPublicSuffix("127.0.0.1").await())
    }
}
