package org.mozilla.gecko.util.publicsuffix;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

@RunWith(TestRunner.class)
public class TestPublicSuffix {
    @Test
    public void testStripPublicSuffix() {
        // Test empty value
        Assert.assertEquals("",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, ""));

        // Test domains with public suffix
        Assert.assertEquals("www.mozilla",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "www.mozilla.org"));
        Assert.assertEquals("www.google",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "www.google.com"));
        Assert.assertEquals("foobar",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "foobar.blogspot.com"));
        Assert.assertEquals("independent",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "independent.co.uk"));
        Assert.assertEquals("biz",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "biz.com.ua"));
        Assert.assertEquals("example",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "example.org"));
        Assert.assertEquals("example",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "example.pvt.k12.ma.us"));

        // Test domain without public suffix
        Assert.assertEquals("localhost",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "localhost"));
        Assert.assertEquals("firefox.mozilla",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "firefox.mozilla"));

        // IDN domains
        Assert.assertEquals("ουτοπία.δπθ",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "ουτοπία.δπθ.gr"));
        Assert.assertEquals("a网络A",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "a网络A.网络.Cn"));

        // Other non-domain values
        Assert.assertEquals("192.168.0.1",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "192.168.0.1"));
        Assert.assertEquals("asdflkj9uahsd",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "asdflkj9uahsd"));

        // Other trailing and other types of dots
        Assert.assertEquals("www.mozilla。home．example",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "www.mozilla。home．example｡org"));
        Assert.assertEquals("example",
                PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, "example.org"));
    }

    @Test(expected = NullPointerException.class)
    public void testStripPublicSuffixThrowsException() {
        PublicSuffix.stripPublicSuffix(RuntimeEnvironment.application, null);
    }
}
