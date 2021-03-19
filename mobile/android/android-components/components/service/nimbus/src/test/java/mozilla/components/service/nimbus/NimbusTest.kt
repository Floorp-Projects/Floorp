/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import android.content.Context
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Response
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.nimbus.GleanMetrics.NimbusEvents
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mozilla.experiments.nimbus.EnrolledExperiment
import org.mozilla.experiments.nimbus.EnrollmentChangeEvent
import org.mozilla.experiments.nimbus.EnrollmentChangeEventType

@RunWith(AndroidJUnit4::class)
class NimbusTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private val appInfo = NimbusAppInfo(
        appName = "NimbusUnitTest",
        channel = "test"
    )

    private val nimbus = Nimbus(
        context = context,
        appInfo = appInfo,
        server = NimbusServerSettings(Uri.parse("https://example.com"))
    )

    @get:Rule
    val gleanRule = GleanTestRule(context)

    @Before
    fun setupGlean() {
        // Glean needs to be initialized for the experiments API to accept enrollment events, so we
        // init it with a mock client so we don't upload anything.
        val mockClient: Client = mock()
        `when`(mockClient.fetch(any())).thenReturn(
            Response("URL", 200, mock(), mock()))
        Glean.initialize(
            context,
            true,
            Configuration(
                httpClient = ConceptFetchHttpUploader(lazy { mockClient })
            )
        )
    }

    @Test
    fun `recordExperimentTelemetry correctly records the experiment and branch`() {
        // Create a list of experiments to test the telemetry enrollment recording
        val enrolledExperiments = listOf(EnrolledExperiment(
            enrollmentId = "enrollment-id",
            slug = "test-experiment",
            featureIds = listOf(),
            branchSlug = "test-branch",
            userFacingDescription = "A test experiment for testing experiments",
            userFacingName = "Test Experiment")
        )

        nimbus.recordExperimentTelemetry(experiments = enrolledExperiments)
        assertTrue(Glean.testIsExperimentActive("test-experiment"))
        val experimentData = Glean.testGetExperimentData("test-experiment")
        assertEquals("test-branch", experimentData.branch)
    }

    @Test
    fun `recordExperimentTelemetryEvents records telemetry`() {
        // Create a bespoke list of events to record, one of each type, all with the same parameters
        val events = listOf(
            EnrollmentChangeEvent(
                experimentSlug = "test-experiment",
                branchSlug = "test-branch",
                enrollmentId = "test-enrollment-id",
                reason = "test-reason",
                change = EnrollmentChangeEventType.ENROLLMENT
            ),
            EnrollmentChangeEvent(
                experimentSlug = "test-experiment",
                branchSlug = "test-branch",
                enrollmentId = "test-enrollment-id",
                reason = "test-reason",
                change = EnrollmentChangeEventType.UNENROLLMENT
            ),
            EnrollmentChangeEvent(
                experimentSlug = "test-experiment",
                branchSlug = "test-branch",
                enrollmentId = "test-enrollment-id",
                reason = "test-reason",
                change = EnrollmentChangeEventType.DISQUALIFICATION
            )
        )

        // Record the experiments in Glean
        nimbus.recordExperimentTelemetryEvents(events)

        // Use the Glean test API to check the recorded metrics

        // Enrollment
        assertTrue("Event must have a value", NimbusEvents.enrollment.testHasValue())
        val enrollmentEvents = NimbusEvents.enrollment.testGetValue()
        assertEquals("Event count must match", enrollmentEvents.count(), 1)
        val enrollmentEventExtras = enrollmentEvents.first().extra!!
        assertEquals("Experiment slug must match", "test-experiment", enrollmentEventExtras["experiment"])
        assertEquals("Experiment branch must match", "test-branch", enrollmentEventExtras["branch"])
        assertEquals("Experiment enrollment-id must match", "test-enrollment-id", enrollmentEventExtras["enrollment_id"])

        // Unenrollment
        assertTrue("Event must have a value", NimbusEvents.unenrollment.testHasValue())
        val unenrollmentEvents = NimbusEvents.unenrollment.testGetValue()
        assertEquals("Event count must match", unenrollmentEvents.count(), 1)
        val unenrollmentEventExtras = unenrollmentEvents.first().extra!!
        assertEquals("Experiment slug must match", "test-experiment", unenrollmentEventExtras["experiment"])
        assertEquals("Experiment branch must match", "test-branch", unenrollmentEventExtras["branch"])
        assertEquals("Experiment enrollment-id must match", "test-enrollment-id", unenrollmentEventExtras["enrollment_id"])

        // Disqualification
        assertTrue("Event must have a value", NimbusEvents.disqualification.testHasValue())
        val disqualificationEvents = NimbusEvents.disqualification.testGetValue()
        assertEquals("Event count must match", disqualificationEvents.count(), 1)
        val disqualificationEventExtras = disqualificationEvents.first().extra!!
        assertEquals("Experiment slug must match", "test-experiment", disqualificationEventExtras["experiment"])
        assertEquals("Experiment branch must match", "test-branch", disqualificationEventExtras["branch"])
        assertEquals("Experiment enrollment-id must match", "test-enrollment-id", disqualificationEventExtras["enrollment_id"])
    }

    @Test
    fun `recordExposure records telemetry`() {
        // Load the experiment in nimbus so and optIn so that it will be active. This is necessary
        // because recordExposure checks for active experiments before recording.
        nimbus.setExperimentsLocallyOnThisThread("""
            {"data": [{
              "schemaVersion": "1.0.0",
              "slug": "test-experiment",
              "endDate": null,
              "branches": [
                {
                  "slug": "test-branch",
                  "ratio": 1
                }
              ],
              "probeSets": [],
              "startDate": null,
              "appName": "NimbusUnitTest",
              "appId": "mozilla.components.service.nimbus.test",
              "channel": "test",
              "bucketConfig": {
                "count": 10000,
                "start": 0,
                "total": 10000,
                "namespace": "test-experiment",
                "randomizationUnit": "nimbus_id"
              },
              "userFacingName": "Diagnostic test experiment",
              "referenceBranch": "test-branch",
              "isEnrollmentPaused": false,
              "proposedEnrollment": 7,
              "userFacingDescription": "This is a test experiment for diagnostic purposes.",
              "id": "test-experiment",
              "last_modified": 1602197324372
            }]}
        """.trimIndent())
        nimbus.applyPendingExperimentsOnThisThread()

        // Record the exposure event in Glean
        nimbus.recordExposureOnThisThread("test-experiment")

        // Use the Glean test API to check the recorded event
        assertTrue("Event must have a value", NimbusEvents.exposure.testHasValue())
        val enrollmentEvents = NimbusEvents.exposure.testGetValue()
        assertEquals("Event count must match", enrollmentEvents.count(), 1)
        val enrollmentEventExtras = enrollmentEvents.first().extra!!
        assertEquals("Experiment slug must match", "test-experiment", enrollmentEventExtras["experiment"])
        assertEquals("Experiment branch must match", "test-branch", enrollmentEventExtras["branch"])
        assertNotNull("Experiment enrollment-id must not be null", enrollmentEventExtras["enrollment_id"])
    }

    @Test
    fun `buildExperimentContext returns a valid context`() {
        val expContext = nimbus.buildExperimentContext(context)
        assertEquals("mozilla.components.service.nimbus.test", expContext.appId)
        // If we could control more of the context here we might be able to better test it
    }

    @Test
    fun `NimbusDisabled is empty`() {
        val nimbus: NimbusApi = NimbusDisabled()
        nimbus.fetchExperiments()
        nimbus.applyPendingExperiments()
        assertTrue("getActiveExperiments should be empty", nimbus.getActiveExperiments().isEmpty())
        assertEquals(null, nimbus.getExperimentBranch("test-experiment"))
    }
}
