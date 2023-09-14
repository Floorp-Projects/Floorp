/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntimeSettings

@RunWith(AndroidJUnit4::class)
@MediumTest
class TrustedRecursiveResolverTest : BaseSessionTest() {

    @Test fun trustedRecursiveResolverMode() {
        val settings = sessionRule.runtime.settings
        val trustedRecursiveResolverModePerf = "network.trr.mode"

        var prefValue = (sessionRule.getPrefs(trustedRecursiveResolverModePerf)[0] as Int)
        assertThat(
            "Initial TRR mode should be TRR_MODE_OFF (0)",
            prefValue,
            `is`(0),
        )

        settings.setTrustedRecursiveResolverMode(GeckoRuntimeSettings.TRR_MODE_FIRST)
        prefValue = (sessionRule.getPrefs(trustedRecursiveResolverModePerf)[0] as Int)

        assertThat(
            "Setting TRR mode to TRR_MODE_FIRST (2)",
            prefValue,
            `is`(2),
        )

        settings.setTrustedRecursiveResolverMode(GeckoRuntimeSettings.TRR_MODE_ONLY)
        prefValue = (sessionRule.getPrefs(trustedRecursiveResolverModePerf)[0] as Int)

        assertThat(
            "Setting TRR mode to TRR_MODE_ONLY (3)",
            prefValue,
            `is`(3),
        )

        settings.setTrustedRecursiveResolverMode(GeckoRuntimeSettings.TRR_MODE_DISABLED)
        prefValue = (sessionRule.getPrefs(trustedRecursiveResolverModePerf)[0] as Int)

        assertThat(
            "Setting TRR mode to TRR_MODE_DISABLED (5)",
            prefValue,
            `is`(5),
        )

        settings.setTrustedRecursiveResolverMode(GeckoRuntimeSettings.TRR_MODE_OFF)
        prefValue = (sessionRule.getPrefs(trustedRecursiveResolverModePerf)[0] as Int)

        assertThat(
            "Setting TRR mode to TRR_MODE_OFF (0)",
            prefValue,
            `is`(0),
        )
    }

    @Test fun trustedRecursiveResolverUrl() {
        val settings = sessionRule.runtime.settings
        val trustedRecursiveResolverUriPerf = "network.trr.uri"

        var prefValue = (sessionRule.getPrefs(trustedRecursiveResolverUriPerf)[0] as String)
        assertThat(
            "Initial TRR Uri should be empty",
            prefValue,
            `is`(""),
        )

        val exampleValue = "https://mozilla.cloudflare-dns.com/dns-query"
        settings.setTrustedRecursiveResolverUri(exampleValue)
        prefValue = (sessionRule.getPrefs(trustedRecursiveResolverUriPerf)[0] as String)
        assertThat(
            "Setting custom TRR Uri should work",
            prefValue,
            `is`(exampleValue),
        )
    }
}
