package org.mozilla.focus.utils;

import junit.framework.Assert;

import org.junit.Test;

public class UrlUtilsTest {
    @Test
    public void urlsMatchExceptForTrailingSlash() throws Exception {
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.mozilla.org", "http://www.mozilla.org"));
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.mozilla.org/", "http://www.mozilla.org"));
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.mozilla.org", "http://www.mozilla.org/"));

        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://mozilla.org", "http://www.mozilla.org"));
        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.mozilla.org/", "http://mozilla.org"));

        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.mozilla.org", "https://www.mozilla.org"));
        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("https://www.mozilla.org", "http://www.mozilla.org"));

        // Same length of domain, but otherwise different:
        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.allizom.org", "http://www.mozilla.org"));
        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.allizom.org/", "http://www.mozilla.org"));
        Assert.assertFalse(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.allizom.org", "http://www.mozilla.org/"));

        // Check upper/lower case is OK:
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.MOZILLA.org", "http://www.mozilla.org"));
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.MOZILLA.org/", "http://www.mozilla.org"));
        Assert.assertTrue(UrlUtils.urlsMatchExceptForTrailingSlash("http://www.MOZILLA.org", "http://www.mozilla.org/"));
    }

}