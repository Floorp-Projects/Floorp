/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

import java.net.URLEncoder;

public class testIntentUri extends UITest {
    public void testIntentUrlFallback() {
        final String targetPage = getAbsoluteHostnameUrl(mStringHelper.ROBOCOP_TEXT_PAGE_URL);
        final String encodedTargetPage = URLEncoder.encode(targetPage);
        final String intentUri = "intent://this.url/wont/be/loaded" +
                "#Intent;scheme=https;package=org.mozilla.notinstalled;" +
                "S.browser_fallback_url=" + encodedTargetPage + ";end";

        GeckoHelper.blockForReady();

        NavigationHelper.enterAndLoadUrl(intentUri);
        mToolbar.assertTitle(targetPage);
    }
}
