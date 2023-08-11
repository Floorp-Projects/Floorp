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
import org.mozilla.geckoview.ExperimentDelegate
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException.ERROR_EXPERIMENT_SLUG_NOT_FOUND
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException.ERROR_FEATURE_NOT_FOUND
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import java.lang.RuntimeException

@RunWith(AndroidJUnit4::class)
@MediumTest
class ExperimentDelegateTest : BaseSessionTest() {

    @Test
    fun withPdfJS() {
        mainSession.loadTestPath(TRACEMONKEY_PDF_PATH)
        sessionRule.addExternalDelegateUntilTestEnd(
            ExperimentDelegate::class,
            sessionRule::setExperimentDelegate,
            { sessionRule.setExperimentDelegate(null) },
            object : ExperimentDelegate {
                @AssertCalled(count = 1)
                override fun onGetExperimentFeature(feature: String): GeckoResult<JSONObject> {
                    assertThat(
                        "Feature id should match",
                        feature,
                        equalTo("pdfjs"),
                    )
                    return GeckoResult<JSONObject>()
                }
            },
        )
    }

    /*
        Basic test of setting a runtime experiment delegate and an example experiment delegate with example functionality usage.
     */
    @Test
    fun experimentDelegateTest() {
        sessionRule.addExternalDelegateUntilTestEnd(
            ExperimentDelegate::class,
            sessionRule::setExperimentDelegate,
            { sessionRule.setExperimentDelegate(null) },
            object : ExperimentDelegate {
                @AssertCalled(count = 1)
                override fun onGetExperimentFeature(feature: String): GeckoResult<JSONObject> {
                    val result = GeckoResult<JSONObject>()
                    if (feature == "test") {
                        result.complete(JSONObject().put("item-one", true).put("item-two", 5))
                    } else {
                        result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
                    }
                    return result
                }

                @AssertCalled(count = 1)
                override fun onRecordExposureEvent(feature: String): GeckoResult<Void> {
                    val result = GeckoResult<Void>()
                    if (feature == "test") {
                        result.complete(null)
                    } else {
                        result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
                    }
                    return result
                }

                @AssertCalled(count = 3)
                override fun onRecordExperimentExposureEvent(feature: String, slug: String): GeckoResult<Void> {
                    val result = GeckoResult<Void>()
                    if (feature == "test" && slug == "test") {
                        result.complete(null)
                    } else if (slug != "test" && feature == "test") {
                        result.completeExceptionally(ExperimentException(ERROR_EXPERIMENT_SLUG_NOT_FOUND))
                    } else {
                        result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
                    }
                    return result
                }

                @AssertCalled(count = 1)
                override fun onRecordMalformedConfigurationEvent(feature: String, part: String): GeckoResult<Void> {
                    val result = GeckoResult<Void>()
                    if (feature == "test") {
                        result.complete(null)
                    } else {
                        result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
                    }
                    return result
                }
            },
        )
        val experimentDelegate = sessionRule.runtime.settings.experimentDelegate!!
        val experimentFeature = sessionRule.waitForResult(experimentDelegate.onGetExperimentFeature("test"))
        assertThat("Experiment item one matches", experimentFeature?.get("item-one"), equalTo(true))
        assertThat("Experiment item two matches", experimentFeature?.get("item-two"), equalTo(5))
        val recordedExposureEvent = sessionRule.waitForResult(experimentDelegate.onRecordExposureEvent("test"))
        assertThat("Recorded an exposure event", recordedExposureEvent, equalTo(null))
        val recordedExperimentExposureEvent = sessionRule.waitForResult(experimentDelegate.onRecordExperimentExposureEvent("test", "test"))
        assertThat("Recorded an experiment exposure event", recordedExperimentExposureEvent, equalTo(null))
        val recordedMalformedEvent = sessionRule.waitForResult(experimentDelegate.onRecordMalformedConfigurationEvent("test", "data"))
        assertThat("Recorded a malformed event", recordedMalformedEvent, equalTo(null))

        try {
            sessionRule.waitForResult(experimentDelegate.onRecordExperimentExposureEvent("test", "no-slug"))
        } catch (re: RuntimeException) {
            // no-op, wait for result for testing throws this
        } catch (ee: ExperimentException) {
            assertThat("Correct error of no slug found.", ee.code, equalTo(ERROR_EXPERIMENT_SLUG_NOT_FOUND))
        }

        try {
            sessionRule.waitForResult(experimentDelegate.onRecordExperimentExposureEvent("no-feature", "test"))
        } catch (re: RuntimeException) {
            // no-op, wait for result  for testing throws this
        } catch (ee: ExperimentException) {
            assertThat("Correct error of no feature found.", ee.code, equalTo(ERROR_FEATURE_NOT_FOUND))
        }
    }
}
