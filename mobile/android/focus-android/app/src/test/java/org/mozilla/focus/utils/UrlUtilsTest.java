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

    @Test
    public void isPermittedResourceProtocol() {
        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("http:"));
        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("https:"));

        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("http://foobar.com"));
        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("https://foobar.com"));

        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("data:???????????????"));
        Assert.assertTrue(UrlUtils.isPermittedResourceProtocol("file:///android_asset/something/index.html"));

        Assert.assertFalse(UrlUtils.isPermittedResourceProtocol("nielsenwebid://nuid/999"));
    }

    @Test
    public void isPermittedProtocol() {
        Assert.assertTrue(UrlUtils.isSupportedProtocol("http:"));
        Assert.assertTrue(UrlUtils.isSupportedProtocol("http://mozilla.org"));
        Assert.assertTrue(UrlUtils.isSupportedProtocol("error:-1"));
        Assert.assertTrue(UrlUtils.isSupportedProtocol("data:?????"));

        Assert.assertFalse(UrlUtils.isSupportedProtocol("market:details?id=org.mozilla.firefox"));
    }
}