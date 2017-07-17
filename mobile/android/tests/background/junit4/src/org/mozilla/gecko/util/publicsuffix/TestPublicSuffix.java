package org.mozilla.gecko.util.publicsuffix;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

    @Test
    public void testGetPublicSuffixZeroAdditionalParts() {
        final Map<String, String> inputToExpected = new HashMap<>();

        // Empty value.
        inputToExpected.put("", "");
        inputToExpected.put(" ", "");

        // Test domains with public suffix
        inputToExpected.put("www.mozilla.org", "org");
        inputToExpected.put("www.google.com", "com");
        inputToExpected.put("foobar.blogspot.com", "blogspot.com");
        inputToExpected.put("independent.co.uk", "co.uk");
        inputToExpected.put("biz.com.ua", "com.ua");
        inputToExpected.put("example.org", "org");
        inputToExpected.put("example.pvt.k12.ma.us", "pvt.k12.ma.us");

        // Test domain without public suffix
        inputToExpected.put("localhost", "");
        inputToExpected.put("firefox.mozilla", "");

        // IDN domains
        inputToExpected.put("ουτοπία.δπθ.gr", "gr");
        inputToExpected.put("a网络A.网络.cn", "网络.cn");

        // Other non-domain values
        inputToExpected.put("192.168.0.1", "");
        inputToExpected.put("asdflkj9uahsd", "");

        // Other trailing and other types of dots
        inputToExpected.put("www.mozilla。home．example｡org", "org");
        inputToExpected.put("example.org", "org");

        for (final Map.Entry<String, String> entry : inputToExpected.entrySet()) {
            final String input = entry.getKey();
            final String expected = entry.getValue();
            Assert.assertEquals("for input:" + input + "||", expected,
                    PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, input, 0));
        }
    }

    @Test
    public void testGetPublicSuffixNonZeroAdditionalParts() {
        final Map<String, String> inputToExpected = new HashMap<>();

        inputToExpected.put("www.mozilla.org", "mozilla.org");
        inputToExpected.put("www.google.com", "google.com");
        inputToExpected.put("bbc.co.uk", "bbc.co.uk");
        inputToExpected.put("www.bbc.co.uk", "bbc.co.uk");

        for (final Map.Entry<String, String> entry : inputToExpected.entrySet()) {
            final String input = entry.getKey();
            final String expected = entry.getValue();
            Assert.assertEquals("for input:" + input + "||", expected,
                    PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, input, 1));
        }

        // More than one additional part.
        Assert.assertEquals("m.bbc.co.uk",
                PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, "www.m.bbc.co.uk", 2));

        // Look for more additional parts than exist in the host: the full host should be returned.
        final List<String> inputsAndExpecteds = new ArrayList<>(Arrays.asList(
                "google.com",
                "www.google.com",
                "bbc.co.uk"
        ));

        for (final String inputAndExpected : inputsAndExpecteds) {
            Assert.assertEquals(inputAndExpected,
                    PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, inputAndExpected, 100));
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetPublicSuffixWithNegativeAdditionalPartCountThrows() {
        PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, "whatever-doesnt-matter", -1);
    }

    @Test(expected = NullPointerException.class)
    public void testGetPublicSuffixWithNullContextThrows() {
        PublicSuffix.getPublicSuffix(null, "whatever", 0);
    }

    @Test(expected = NullPointerException.class)
    public void testGetPublicSuffixWithNullDomainThrows() {
        PublicSuffix.getPublicSuffix(RuntimeEnvironment.application, null, 0);
    }
}
