/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.json.JSONObject
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate

@RunWith(AndroidJUnit4::class)
@MediumTest
class NimbusTest : BaseSessionTest() {

    @Test
    fun withPdfJS() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)

        sessionRule.waitUntilCalled(object : ContentDelegate {
            override fun onGetNimbusFeature(session: GeckoSession, featureId: String): JSONObject? {
                assertThat(
                    "Feature id should match",
                    featureId,
                    equalTo("pdfjs"),
                )
                return null
            }
        })
    }
}
