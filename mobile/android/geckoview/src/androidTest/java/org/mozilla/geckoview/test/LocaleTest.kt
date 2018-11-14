/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.ReuseSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.WithDevToolsAPI

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@MediumTest
@RunWith(AndroidJUnit4::class)
@WithDevToolsAPI
class LocaleTest : BaseSessionTest() {

    @Test fun setLocale() {
        sessionRule.runtime.getSettings().setLocale("en-GB");

        val index = sessionRule.waitForChromeJS(String.format(
                "(function() {" +
                "  return ChromeUtils.import('resource://gre/modules/Services.jsm', {})" +
                "    .Services.locale.requestedLocales.indexOf('en-GB');" +
                "})()")) as Double;

        assertThat("Requested locale is found", index, greaterThanOrEqualTo(0.0));
    }
}
