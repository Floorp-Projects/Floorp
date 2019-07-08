/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

@MediumTest
@RunWith(AndroidJUnit4::class)
class LocaleTest : BaseSessionTest() {

    @Test fun setLocale() {
        sessionRule.runtime.settings.setLocales(arrayOf("en-GB"));
        assertThat("Requested locale is found", sessionRule.requestedLocales.indexOf("en-GB"),
                greaterThanOrEqualTo(0))
    }
}
