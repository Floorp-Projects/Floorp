/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith

@MediumTest
@RunWith(AndroidJUnit4::class)
class LocaleTest : BaseSessionTest() {

    @Test fun setLocale() {
        sessionRule.runtime.settings.setLocales(arrayOf("en-GB"))
        assertThat(
            "Requested locale is found",
            sessionRule.requestedLocales.indexOf("en-GB"),
            greaterThanOrEqualTo(0),
        )
    }

    @Test fun duplicateLocales() {
        sessionRule.runtime.settings.setLocales(arrayOf("en-gb", "en-US", "en-gb", "en-fr", "en-us", "en-FR"))
        assertThat(
            "Locales have no duplicates",
            sessionRule.requestedLocales,
            equalTo(listOf("en-GB", "en-US", "en-FR")),
        )
    }

    @Test fun lowerCaseToUpperCaseLocales() {
        sessionRule.runtime.settings.setLocales(arrayOf("en-gb", "en-us", "en-fr"))
        assertThat(
            "Locales are formatted properly",
            sessionRule.requestedLocales,
            equalTo(listOf("en-GB", "en-US", "en-FR")),
        )
    }

    @Test
    fun acceptLangaugeFormat() {
        // No way to override default language settings from unit test.
        // So we only test this on current settings.

        val intlAcceptLanauge = "intl.accept_languages"
        val prefValue = (sessionRule.getPrefs(intlAcceptLanauge)[0] as String).split(",")
        for (value in prefValue) {
            assertThat("Accept-Lanauge format should be language or language-region", value.filter { it == '-' }.count(), lessThanOrEqualTo(1))
        }
    }
}
