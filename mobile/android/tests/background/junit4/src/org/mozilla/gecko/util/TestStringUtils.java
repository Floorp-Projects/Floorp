/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

@RunWith(TestRunner.class)
public class TestStringUtils {
    @Test
    public void testIsHttpOrHttps() {
        // No value
        Assert.assertFalse(StringUtils.isHttpOrHttps(null));
        Assert.assertFalse(StringUtils.isHttpOrHttps(""));

        // Garbage
        Assert.assertFalse(StringUtils.isHttpOrHttps("lksdjflasuf"));

        // URLs with http/https
        Assert.assertTrue(StringUtils.isHttpOrHttps("https://www.google.com"));
        Assert.assertTrue(StringUtils.isHttpOrHttps("http://www.facebook.com"));
        Assert.assertTrue(StringUtils.isHttpOrHttps("https://mozilla.org/en-US/firefox/products/"));

        // IP addresses
        Assert.assertTrue(StringUtils.isHttpOrHttps("https://192.168.0.1"));
        Assert.assertTrue(StringUtils.isHttpOrHttps("http://63.245.215.20/en-US/firefox/products"));

        // Other protocols
        Assert.assertFalse(StringUtils.isHttpOrHttps("ftp://people.mozilla.org"));
        Assert.assertFalse(StringUtils.isHttpOrHttps("javascript:window.google.com"));
        Assert.assertFalse(StringUtils.isHttpOrHttps("tel://1234567890"));

        // No scheme
        Assert.assertFalse(StringUtils.isHttpOrHttps("google.com"));
        Assert.assertFalse(StringUtils.isHttpOrHttps("git@github.com:mozilla/gecko-dev.git"));
    }
}
