/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.checkPingSchema
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.getContextWithMockedInfo
import mozilla.components.service.glean.getMockWebServer
import mozilla.components.service.glean.getWorkerStatus
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.triggerWorkManager
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.service.glean.utils.parseISOTimeString
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.concurrent.TimeUnit as AndroidTimeUnit

@RunWith(RobolectricTestRunner::class)
class MetricsPingSchedulerTest {
    private fun <T> kotlinFriendlyAny(): T {
        // This is required to work around the Kotlin/ArgumentMatchers problem with using
        // `verify` on non-nullable arguments (since `any` may return null). See
        // https://github.com/nhaarman/mockito-kotlin/issues/241
        ArgumentMatchers.any<T>()
        // This weird cast was suggested as part of https://youtrack.jetbrains.com/issue/KT-8135 .
        // See the related issue for why this is needed in mocks.
        @Suppress("UNCHECKED_CAST")
        return null as T
    }

    @Before
    fun setup() {
        WorkManagerTestInitHelper.initializeTestWorkManager(
            ApplicationProvider.getApplicationContext())

        Glean.enableTestingMode()
    }

    @Test
    fun `milliseconds until the due time must be correctly computed`() {
        val metricsPingScheduler = MetricsPingScheduler(
            ApplicationProvider.getApplicationContext())

        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 3, 0, 0)

        // We expect the function to return 1 hour, in milliseconds.
        assertEquals(60 * 60 * 1000,
            metricsPingScheduler.getMillisecondsUntilDueTime(
                sendTheNextCalendarDay = false, now = fakeNow, dueHourOfTheDay = 4)
        )

        // If we're exactly at 4am, there must be no delay.
        fakeNow.set(2015, 6, 11, 4, 0, 0)
        assertEquals(0,
            metricsPingScheduler.getMillisecondsUntilDueTime(
                sendTheNextCalendarDay = false, now = fakeNow, dueHourOfTheDay = 4)
        )

        // Set the clock to after 4 of some minutes.
        fakeNow.set(2015, 6, 11, 4, 5, 0)

        // Since `sendTheNextCalendarDay` is false, this will be overdue, returning 0.
        assertEquals(0,
            metricsPingScheduler.getMillisecondsUntilDueTime(
                sendTheNextCalendarDay = false, now = fakeNow, dueHourOfTheDay = 4)
        )

        // With `sendTheNextCalendarDay` true, we expect the function to return 23 hours
        // and 55 minutes, in milliseconds.
        assertEquals(23 * 60 * 60 * 1000 + 55 * 60 * 1000,
            metricsPingScheduler.getMillisecondsUntilDueTime(
                sendTheNextCalendarDay = true, now = fakeNow, dueHourOfTheDay = 4)
        )
    }

    @Test
    fun `getDueTimeForToday must correctly return the due time for the current day`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())

        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 3, 0, 0)

        val expected = Calendar.getInstance()
        expected.time = fakeNow.time
        expected.set(Calendar.HOUR_OF_DAY, 4)

        assertEquals(expected, mps.getDueTimeForToday(fakeNow, 4))

        // Let's check what happens at "midnight".
        fakeNow.set(2015, 6, 11, 0, 0, 0)
        assertEquals(expected, mps.getDueTimeForToday(fakeNow, 4))
    }

    @Test
    fun `isAfterDueTime must report false before the due time on the same calendar day`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())

        val fakeNow = Calendar.getInstance()
        fakeNow.clear()

        // Shortly before.
        fakeNow.set(2015, 6, 11, 3, 0, 0)
        assertFalse(mps.isAfterDueTime(fakeNow, 4))

        // The same hour.
        fakeNow.set(2015, 6, 11, 4, 0, 0)
        assertFalse(mps.isAfterDueTime(fakeNow, 4))

        // Midnight.
        fakeNow.set(2015, 6, 11, 0, 0, 0)
        assertFalse(mps.isAfterDueTime(fakeNow, 4))
    }

    @Test
    fun `isAfterDueTime must report true after the due time on the same calendar day`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())

        val fakeNow = Calendar.getInstance()
        fakeNow.clear()

        // Shortly after.
        fakeNow.set(2015, 6, 11, 4, 1, 0)
        assertTrue(mps.isAfterDueTime(fakeNow, 4))
    }

    @Test
    fun `getLastCollectedDate must report null when no stored date is available`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())
        mps.sharedPreferences.edit().clear().apply()

        assertNull(
            "null must be reported when no date is stored",
            mps.getLastCollectedDate()
        )
    }

    @Test
    fun `getLastCollectedDate must report null when the stored date is corrupted`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())
        mps.sharedPreferences
            .edit()
            .putLong(MetricsPingScheduler.LAST_METRICS_PING_SENT_DATETIME, 123L)
            .apply()

        // Wrong key type should trigger returning null.
        assertNull(
            "null must be reported when no date is stored",
            mps.getLastCollectedDate()
        )

        // Wrong date format string should trigger returning null.
        mps.sharedPreferences
            .edit()
            .putString(MetricsPingScheduler.LAST_METRICS_PING_SENT_DATETIME, "not-an-ISO-date")
            .apply()

        assertNull(
            "null must be reported when the date key is of unexpected format",
            mps.getLastCollectedDate()
        )
    }

    @Test
    fun `getLastCollectedDate must report the stored last collected date, if available`() {
        val testDate = "2018-12-19T12:36:00-06:00"
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())
        mps.updateSentDate(testDate)

        val expectedDate = parseISOTimeString(testDate)!!
        assertEquals(
            "The date the ping was collected must be reported",
            expectedDate,
            mps.getLastCollectedDate()
        )
    }

    @Test
    fun `collectMetricsPing must update the last sent date and reschedule the collection`() {
        val mpsSpy = spy(
            MetricsPingScheduler(ApplicationProvider.getApplicationContext()))

        // Ensure we have the right assumptions in place: the methods were not called
        // prior to |collectPingAndReschedule|.
        verify(mpsSpy, times(0)).updateSentDate(anyString())
        verify(mpsSpy, times(0)).schedulePingCollection(
            kotlinFriendlyAny(),
            anyBoolean()
        )

        mpsSpy.collectPingAndReschedule(Calendar.getInstance())

        // Verify that we correctly called in the methods.
        verify(mpsSpy, times(1)).updateSentDate(anyString())
        verify(mpsSpy, times(1)).schedulePingCollection(
            kotlinFriendlyAny(),
            anyBoolean()
        )
    }

    @Test
    fun `collectMetricsPing must correctly trigger the collection of the metrics ping`() {
        // Setup a test server and make Glean point to it.
        val server = getMockWebServer()

        resetGlean(getContextWithMockedInfo(), Configuration(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        try {
            // Setup a testing metric and set it to some value.
            val testMetric = StringMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "string_metric",
                sendInPings = listOf("metrics")
            )

            val expectedValue = "test-only metric"
            testMetric.set(expectedValue)
            assertTrue("The initial test data must have been recorded", testMetric.testHasValue())

            // Manually call the function to trigger the collection.
            Glean.metricsPingScheduler.collectPingAndReschedule(Calendar.getInstance())

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            // Fetch the ping from the server and decode its JSON body.
            val request = server.takeRequest(20L, AndroidTimeUnit.SECONDS)
            val metricsJsonData = request.body.readUtf8()
            val metricsJson = JSONObject(metricsJsonData)

            // Validate the received data.
            checkPingSchema(metricsJson)
            assertEquals("The received ping must be a 'metrics' ping",
                "metrics", metricsJson.getJSONObject("ping_info")["ping_type"])
            assertEquals(
                "The reported metric must contain the expected value",
                expectedValue,
                metricsJson.getJSONObject("metrics")
                    .getJSONObject("string")
                    .getString("telemetry.string_metric")
            )
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `startupCheck must immediately collect if the ping is overdue for today`() {
        // Set the current system time to a known datetime.
        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 7, 0, 0)

        // Set the last sent date to a previous day, so that today's ping is overdue.
        val mpsSpy =
            spy(MetricsPingScheduler(ApplicationProvider.getApplicationContext()))
        val overdueTestDate = "2015-07-05T12:36:00-06:00"
        mpsSpy.updateSentDate(overdueTestDate)

        MetricsPingScheduler.isInForeground = true

        verify(mpsSpy, never()).collectPingAndReschedule(kotlinFriendlyAny())

        // Make sure to return the fake date when requested.
        doReturn(fakeNow).`when`(mpsSpy).getCalendarInstance()

        // Trigger the startup check. We need to wrap this in `blockDispatchersAPI` since
        // the immediate startup collection happens in the Dispatchers.API context. If we
        // don't, test will fail due to async weirdness.
        mpsSpy.schedule()

        // And that we're storing the current date (this only reports the date, not the time).
        fakeNow.set(Calendar.HOUR_OF_DAY, 0)
        assertEquals(fakeNow.time, mpsSpy.getLastCollectedDate())

        // Verify that we're immediately collecting.
        verify(mpsSpy, times(1)).collectPingAndReschedule(fakeNow)
    }

    @Test
    fun `startupCheck must schedule collection for the next calendar day if collection already happened`() {
        // Set the current system time to a known datetime.
        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 7, 0, 0)
        SystemClock.setCurrentTimeMillis(fakeNow.timeInMillis)

        // Set the last sent date to now.
        val mpsSpy =
            spy(MetricsPingScheduler(ApplicationProvider.getApplicationContext()))
        mpsSpy.updateSentDate(getISOTimeString(fakeNow, truncateTo = TimeUnit.Day))

        verify(mpsSpy, never()).schedulePingCollection(kotlinFriendlyAny(), anyBoolean())

        // Make sure to return the fake date when requested.
        doReturn(fakeNow).`when`(mpsSpy).getCalendarInstance()

        // Trigger the startup check.
        mpsSpy.schedule()

        // Verify that we're scheduling for the next day and not collecting immediately.
        verify(mpsSpy, times(1)).schedulePingCollection(fakeNow, sendTheNextCalendarDay = true)
        verify(mpsSpy, never()).schedulePingCollection(fakeNow, sendTheNextCalendarDay = false)
        verify(mpsSpy, never()).collectPingAndReschedule(kotlinFriendlyAny())
    }

    @Test
    fun `startupCheck must schedule collection for later today if it's before the due time`() {
        // Set the current system time to a known datetime.
        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 2, 0, 0)
        SystemClock.setCurrentTimeMillis(fakeNow.timeInMillis)

        // Set the last sent date to yesterday.
        val mpsSpy =
            spy(MetricsPingScheduler(ApplicationProvider.getApplicationContext()))

        val fakeYesterday = Calendar.getInstance()
        fakeYesterday.time = fakeNow.time
        fakeYesterday.add(Calendar.DAY_OF_MONTH, -1)
        mpsSpy.updateSentDate(getISOTimeString(fakeYesterday, truncateTo = TimeUnit.Day))

        // Make sure to return the fake date when requested.
        doReturn(fakeNow).`when`(mpsSpy).getCalendarInstance()

        verify(mpsSpy, never()).schedulePingCollection(kotlinFriendlyAny(), anyBoolean())

        // Trigger the startup check.
        mpsSpy.schedule()

        // Verify that we're scheduling for today, but not collecting immediately.
        verify(mpsSpy, times(1)).schedulePingCollection(fakeNow, sendTheNextCalendarDay = false)
        verify(mpsSpy, never()).schedulePingCollection(fakeNow, sendTheNextCalendarDay = true)
        verify(mpsSpy, never()).collectPingAndReschedule(kotlinFriendlyAny())
    }

    @Test
    fun `startupCheck must correctly handle fresh installs (before due time)`() {
        // Set the current system time to a known datetime: before 4am local.
        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 3, 0, 0)

        // Clear the last sent date.
        val mpsSpy =
            spy(MetricsPingScheduler(ApplicationProvider.getApplicationContext()))
        mpsSpy.sharedPreferences.edit().clear().apply()

        verify(mpsSpy, never()).collectPingAndReschedule(kotlinFriendlyAny())

        // Make sure to return the fake date when requested.
        doReturn(fakeNow).`when`(mpsSpy).getCalendarInstance()

        // Trigger the startup check.
        mpsSpy.schedule()

        // Verify that we're immediately collecting.
        verify(mpsSpy, never()).collectPingAndReschedule(fakeNow)
        verify(mpsSpy, times(1)).schedulePingCollection(fakeNow, sendTheNextCalendarDay = false)
    }

    @Test
    fun `startupCheck must correctly handle fresh installs (after due time)`() {
        // Set the current system time to a known datetime: after 4am local.
        val fakeNow = Calendar.getInstance()
        fakeNow.clear()
        fakeNow.set(2015, 6, 11, 6, 0, 0)

        // Clear the last sent date.
        val mpsSpy =
            spy(MetricsPingScheduler(ApplicationProvider.getApplicationContext()))
        mpsSpy.sharedPreferences.edit().clear().apply()

        verify(mpsSpy, never()).collectPingAndReschedule(kotlinFriendlyAny())

        // Make sure to return the fake date when requested.
        doReturn(fakeNow).`when`(mpsSpy).getCalendarInstance()

        // Trigger the startup check.
        mpsSpy.schedule()

        // And that we're storing the current date (this only reports the date, not the time).
        fakeNow.set(Calendar.HOUR_OF_DAY, 0)
        assertEquals(
            "The scheduler must save the date the ping was collected",
            fakeNow.time,
            mpsSpy.getLastCollectedDate()
        )

        // Verify that we're immediately collecting.
        verify(mpsSpy, times(1)).collectPingAndReschedule(fakeNow)
        verify(mpsSpy, never()).schedulePingCollection(fakeNow, sendTheNextCalendarDay = false)
    }

    @Test
    fun `schedulePingCollection must correctly append a work request to the WorkManager`() {
        // Replacing the singleton's metricsPingScheduler here since doWork() refers to it when
        // the worker runs, otherwise we can get a lateinit property is not initialized error.
        Glean.metricsPingScheduler = MetricsPingScheduler(ApplicationProvider.getApplicationContext())
        MetricsPingScheduler.isInForeground = true

        // No work should be enqueued at the beginning of the test.
        assertFalse(getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)

        // Manually schedule a collection task for today.
        Glean.metricsPingScheduler.schedulePingCollection(Calendar.getInstance(), sendTheNextCalendarDay = false)

        // We expect the worker to be scheduled.
        assertTrue(getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)

        resetGlean(clearStores = true)
    }

    @Test
    fun `schedule() happens when returning from background when Glean is already initialized`() {
        // Initialize Glean
        resetGlean()

        // We expect the worker to not be scheduled.
        assertFalse(getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)

        // Simulate returning to the foreground with glean initialized.
        Glean.metricsPingScheduler.onEnterForeground()

        // We expect the worker to be scheduled.
        assertTrue(getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)
    }

    @Test
    fun `cancel() correctly cancels worker`() {
        val mps = MetricsPingScheduler(ApplicationProvider.getApplicationContext())

        mps.schedulePingCollection(Calendar.getInstance(), true)

        // Verify that the worker is enqueued
        assertTrue("MetricsPingWorker is enqueued",
            getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)

        // Cancel the worker
        MetricsPingScheduler.cancel()

        // Verify worker has been cancelled
        assertFalse("MetricsPingWorker is not enqueued",
            getWorkerStatus(MetricsPingWorker.TAG).isEnqueued)
    }

    // @Test
    // fun `Glean should close the measurement window for overdue pings before recording new data`() {
    //     // This test is a bit tricky: we want to make sure that, when our metrics ping is overdue
    //     // and collected at startup (if there's pending data), we don't mistakenly add new collected
    //     // data to it. In order to test for this specific edge case, we resort to the following:
    //     // record some data, then "pretend Glean is disabled" to simulate a crash, start using the
    //     // recording API off the main thread, init Glean in a separate thread and trigger a metrics
    //     // ping at startup. We expect the initially written data to be there ("expected_data!"), but
    //     // not the "should_not_be_recorded", which will be reported in a separate ping.

    //     // Create a string metric with a Ping lifetime.
    //     val stringMetric = StringMetricType(
    //         disabled = false,
    //         category = "telemetry",
    //         lifetime = Lifetime.Ping,
    //         name = "string_metric",
    //         sendInPings = listOf("metrics")
    //     )

    //     // Start Glean in the current thread, clean the local storage.
    //     resetGlean()

    //     // Record the data we expect to be in the final metrics ping.
    //     val expectedValue = "expected_data!"
    //     stringMetric.set(expectedValue)
    //     assertTrue("The initial expected data must be recorded", stringMetric.testHasValue())

    //     // Pretend Glean is disabled. This is used so that any API call will be discarded and
    //     // Glean will init again.
    //     Glean.initialized = false

    //     // Start the web-server that will receive the metrics ping.
    //     val server = getMockWebServer()

    //     // Set the current system time to a known datetime: this should make the metrics ping
    //     // overdue and trigger it at startup.
    //     val fakeNow = Calendar.getInstance()
    //     fakeNow.clear()
    //     fakeNow.set(2015, 6, 11, 7, 0, 0)
    //     SystemClock.setCurrentTimeMillis(fakeNow.timeInMillis)

    //     // Start recording to the metric asynchronously, from a separate thread. If something
    //     // goes wrong with our init, we should see the value set in the loop below in the triggered
    //     // "metrics" ping.
    //     var stopWrites = false
    //     val stringWriter = GlobalScope.async {
    //         do {
    //             stringMetric.set("should_not_be_recorded")
    //         } while (!stopWrites)
    //     }

    //     try {
    //         // Restart Glean in a separate thread to simulate a crash/restart without
    //         // clearing the local storage.
    //         val asyncInit = GlobalScope.async {
    //             Glean.initialize(getContextWithMockedInfo(), Configuration(
    //                 serverEndpoint = "http://" + server.hostName + ":" + server.port,
    //                 logPings = true
    //             ))

    //             // Trigger worker task to upload the pings in the background.
    //             triggerWorkManager()
    //         }

    //         // Wait for the metrics ping to be received.
    //         val request = server.takeRequest(20L, AndroidTimeUnit.SECONDS)

    //         // Stop recording to the test metric and wait for the async stuff
    //         // to complete.
    //         runBlocking {
    //             stopWrites = true
    //             stringWriter.await()
    //             asyncInit.await()
    //         }

    //         // Parse the received ping payload to a JSON object.
    //         val metricsJsonData = request.body.readUtf8()
    //         val metricsJson = JSONObject(metricsJsonData)

    //         // Validate the received data.
    //         checkPingSchema(metricsJson)
    //         assertEquals("The received ping must be a 'metrics' ping",
    //             "metrics", metricsJson.getJSONObject("ping_info")["ping_type"])
    //         assertEquals(
    //             "The reported metric must contain the expected value",
    //             expectedValue,
    //             metricsJson.getJSONObject("metrics")
    //                 .getJSONObject("string")
    //                 .getString("telemetry.string_metric")
    //         )
    //     } finally {
    //         server.shutdown()
    //     }
    // }
}
