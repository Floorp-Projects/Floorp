/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.Context;
import android.os.AsyncTask;
import android.os.PersistableBundle;
import android.preference.PreferenceManager;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement;
import org.mozilla.telemetry.measurement.SearchesMeasurement;
import org.mozilla.telemetry.net.HttpURLConnectionTelemetryClient;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.schedule.jobscheduler.JobSchedulerTelemetryScheduler;
import org.mozilla.telemetry.schedule.jobscheduler.TelemetryJobService;
import org.mozilla.telemetry.serialize.JSONPingSerializer;
import org.mozilla.telemetry.serialize.TelemetryPingSerializer;
import org.mozilla.telemetry.storage.FileTelemetryStorage;
import org.mozilla.telemetry.storage.TelemetryStorage;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.List;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
@Config(sdk = android.os.Build.VERSION_CODES.LOLLIPOP)
public class TelemetryTest {
    private static final String TEST_USER_AGENT = "Test/42.0.23";
    private static final String TEST_SETTING_1 = "test-setting-1";
    private static final String TEST_SETTING_2 = "test-setting-2";

    @Test
    public void testCorePingIntegration() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setAppName("TelemetryTest")
                .setAppVersion("10.0.1")
                .setUpdateChannel("test")
                .setBuildId("123")
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort())
                .setUserAgent(TEST_USER_AGENT);

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new HttpURLConnectionTelemetryClient());
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final TelemetryCorePingBuilder pingBuilder = spy(new TelemetryCorePingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(pingBuilder).generateDocumentId();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder);
        TelemetryHolder.set(telemetry);

        telemetry.recordSessionStart();
        Thread.sleep(2100);
        telemetry.recordSessionEnd();

        telemetry.recordActiveExperiments(Arrays.asList("first-experiment-id", "second-experiment-id"));

        telemetry.queuePing(TelemetryCorePingBuilder.TYPE);
        telemetry.scheduleUpload();

        TestUtils.waitForExecutor(telemetry);

        assertEquals(1, storage.countStoredPings(TelemetryCorePingBuilder.TYPE));

        assertJobIsScheduled();
        executePendingJob(TelemetryCorePingBuilder.TYPE);

        verify(client).uploadPing(eq(configuration), anyString(), anyString());

        assertEquals(0, storage.countStoredPings(TelemetryCorePingBuilder.TYPE));

        final RecordedRequest request = server.takeRequest();
        assertEquals("POST /submit/telemetry/ffffffff-0000-0000-ffff-ffffffffffff/core/TelemetryTest/10.0.1/test/123?v=4 HTTP/1.1", request.getRequestLine());
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"));
        assertEquals(TEST_USER_AGENT, request.getHeader("User-Agent"));
        assertNotNull(request.getHeader("Date"));

        final JSONObject object = new JSONObject(request.getBody().readUtf8());

        assertTrue(object.has("v"));
        assertTrue(object.has("clientId"));
        assertTrue(object.has("seq"));
        assertTrue(object.has("locale"));
        assertTrue(object.has("os"));
        assertTrue(object.has("device"));
        assertTrue(object.has("created"));
        assertTrue(object.has("tz"));
        assertTrue(object.has("sessions"));
        assertTrue(object.has("durations"));
        assertTrue(object.has("experiments"));

        assertEquals(1, object.getInt("sessions"));
        assertTrue(object.getInt("durations") > 0);

        final JSONArray experimentsArray = object.getJSONArray("experiments");
        assertEquals(2, experimentsArray.length());
        assertEquals("first-experiment-id", experimentsArray.get(0));
        assertEquals("second-experiment-id", experimentsArray.get(1));

        server.shutdown();
    }

    @Test
    public void testEventPingIntegration() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application)
                .edit()
                .putBoolean(TEST_SETTING_1, true)
                .putString(TEST_SETTING_2, "mozilla")
                .apply();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort())
                .setUserAgent(TEST_USER_AGENT)
                .setAppName("TelemetryTest")
                .setAppVersion("12.1.1")
                .setUpdateChannel("test")
                .setBuildId("456")
                .setPreferencesImportantForTelemetry(TEST_SETTING_1, TEST_SETTING_2);

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new HttpURLConnectionTelemetryClient());
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final TelemetryMobileEventPingBuilder pingBuilder = spy(new TelemetryMobileEventPingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(pingBuilder).generateDocumentId();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder);
        TelemetryHolder.set(telemetry);

        TelemetryEvent.create("action", "type_url", "search_bar").queue();
        TelemetryEvent.create("action", "type_query", "search_bar").queue();
        TelemetryEvent.create("action", "click", "erase_button").queue();

        telemetry.queuePing(TelemetryMobileEventPingBuilder.TYPE);

        TestUtils.waitForExecutor(telemetry);

        assertEquals(1, storage.countStoredPings(TelemetryMobileEventPingBuilder.TYPE));

        telemetry.scheduleUpload();

        TestUtils.waitForExecutor(telemetry);

        assertJobIsScheduled();
        executePendingJob(TelemetryMobileEventPingBuilder.TYPE);

        verify(client).uploadPing(eq(configuration), anyString(), anyString());

        assertEquals(0, storage.countStoredPings(TelemetryMobileEventPingBuilder.TYPE));

        final RecordedRequest request = server.takeRequest();
        assertEquals("POST /submit/telemetry/ffffffff-0000-0000-ffff-ffffffffffff/mobile-event/TelemetryTest/12.1.1/test/456?v=4 HTTP/1.1", request.getRequestLine());
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"));
        assertEquals(TEST_USER_AGENT, request.getHeader("User-Agent"));
        assertNotNull(request.getHeader("Date"));

        final JSONObject object = new JSONObject(request.getBody().readUtf8());

        assertTrue(object.has("v"));
        assertTrue(object.has("clientId"));
        assertTrue(object.has("seq"));
        assertTrue(object.has("locale"));
        assertTrue(object.has("os"));
        assertTrue(object.has("osversion"));
        assertTrue(object.has("created"));
        assertTrue(object.has("tz"));
        assertTrue(object.has("settings"));
        assertTrue(object.has("events"));

        final JSONObject settings = object.getJSONObject("settings");
        assertEquals(2, settings.length());
        assertEquals("true", settings.getString(TEST_SETTING_1));
        assertEquals("mozilla", settings.getString(TEST_SETTING_2));

        final JSONArray events = object.getJSONArray("events");
        assertEquals(3, events.length());

        final JSONArray event1 = events.getJSONArray(0);
        assertEquals("action", event1.getString(1));
        assertEquals("type_url", event1.getString(2));
        assertEquals("search_bar", event1.getString(3));

        final JSONArray event2 = events.getJSONArray(1);
        assertEquals("action", event2.getString(1));
        assertEquals("type_query", event2.getString(2));
        assertEquals("search_bar", event2.getString(3));

        server.shutdown();
    }

    @Test
    @Deprecated // If you change this test, change the one above it.
    public void testEventPingIntegrationLegacyPingType() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application)
                .edit()
                .putBoolean(TEST_SETTING_1, true)
                .putString(TEST_SETTING_2, "mozilla")
                .apply();

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort())
                .setUserAgent(TEST_USER_AGENT)
                .setAppName("TelemetryTest")
                .setAppVersion("12.1.1")
                .setUpdateChannel("test")
                .setBuildId("456")
                .setPreferencesImportantForTelemetry(TEST_SETTING_1, TEST_SETTING_2);

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new HttpURLConnectionTelemetryClient());
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final TelemetryEventPingBuilder pingBuilder = spy(new TelemetryEventPingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(pingBuilder).generateDocumentId();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder);
        TelemetryHolder.set(telemetry);

        TelemetryEvent.create("action", "type_url", "search_bar").queue();
        TelemetryEvent.create("action", "type_query", "search_bar").queue();
        TelemetryEvent.create("action", "click", "erase_button").queue();

        telemetry.queuePing(TelemetryEventPingBuilder.TYPE);

        TestUtils.waitForExecutor(telemetry);

        assertEquals(1, storage.countStoredPings(TelemetryEventPingBuilder.TYPE));

        telemetry.scheduleUpload();

        TestUtils.waitForExecutor(telemetry);

        assertJobIsScheduled();
        executePendingJob(TelemetryEventPingBuilder.TYPE);

        verify(client).uploadPing(eq(configuration), anyString(), anyString());

        assertEquals(0, storage.countStoredPings(TelemetryEventPingBuilder.TYPE));

        final RecordedRequest request = server.takeRequest();
        assertEquals("POST /submit/telemetry/ffffffff-0000-0000-ffff-ffffffffffff/focus-event/TelemetryTest/12.1.1/test/456?v=4 HTTP/1.1", request.getRequestLine());
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"));
        assertEquals(TEST_USER_AGENT, request.getHeader("User-Agent"));
        assertNotNull(request.getHeader("Date"));

        final JSONObject object = new JSONObject(request.getBody().readUtf8());

        assertTrue(object.has("v"));
        assertTrue(object.has("clientId"));
        assertTrue(object.has("seq"));
        assertTrue(object.has("locale"));
        assertTrue(object.has("os"));
        assertTrue(object.has("osversion"));
        assertTrue(object.has("created"));
        assertTrue(object.has("tz"));
        assertTrue(object.has("settings"));
        assertTrue(object.has("events"));

        final JSONObject settings = object.getJSONObject("settings");
        assertEquals(2, settings.length());
        assertEquals("true", settings.getString(TEST_SETTING_1));
        assertEquals("mozilla", settings.getString(TEST_SETTING_2));

        final JSONArray events = object.getJSONArray("events");
        assertEquals(3, events.length());

        final JSONArray event1 = events.getJSONArray(0);
        assertEquals("action", event1.getString(1));
        assertEquals("type_url", event1.getString(2));
        assertEquals("search_bar", event1.getString(3));

        final JSONArray event2 = events.getJSONArray(1);
        assertEquals("action", event2.getString(1));
        assertEquals("type_query", event2.getString(2));
        assertEquals("search_bar", event2.getString(3));

        server.shutdown();
    }

    @Test
    public void testCorePingWithSearches() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort());

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new HttpURLConnectionTelemetryClient());
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(new TelemetryCorePingBuilder(configuration));
        TelemetryHolder.set(telemetry);

        telemetry.setDefaultSearchProvider(new DefaultSearchMeasurement.DefaultSearchEngineProvider() {
            @Override
            public String getDefaultSearchEngineIdentifier() {
                return "test-default-search";
            }
        });

        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "google");
        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "yahoo");
        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "yahoo");
        telemetry.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "duckduckgo");
        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "duckduckgo");

        telemetry.queuePing(TelemetryCorePingBuilder.TYPE);
        TestUtils.waitForExecutor(telemetry);

        telemetry.scheduleUpload();
        TestUtils.waitForExecutor(telemetry);

        executePendingJob(TelemetryCorePingBuilder.TYPE);

        final RecordedRequest request = server.takeRequest();

        final JSONObject object = new JSONObject(request.getBody().readUtf8());

        assertTrue(object.has("searches"));
        assertTrue(object.has("defaultSearch"));

        assertEquals("test-default-search", object.getString("defaultSearch"));

        final JSONObject searches = object.getJSONObject("searches");

        assertTrue(searches.has("actionbar.google"));
        assertTrue(searches.has("actionbar.yahoo"));
        assertTrue(searches.has("actionbar.duckduckgo"));
        assertTrue(searches.has("suggestion.duckduckgo"));

        assertEquals(1, searches.getInt("actionbar.google"));
        assertEquals(2, searches.getInt("actionbar.yahoo"));
        assertEquals(1, searches.getInt("actionbar.duckduckgo"));
        assertEquals(1, searches.getInt("actionbar.duckduckgo"));

        server.shutdown();
    }

    @Test
    public void testPingIsQueuedIfEventLimitIsReached() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        // This test assumes the maximum number of events is 500
        assertEquals(500, configuration.getMaximumNumberOfEventsPerPing());

        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final TelemetryMobileEventPingBuilder pingBuilder = new TelemetryMobileEventPingBuilder(configuration);
        final Telemetry telemetry = spy(new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder));

        TelemetryHolder.set(telemetry);

        for (int i = 0; i < 499; i++) {
            TelemetryEvent.create("category", "method", "object", String.valueOf(i)).queue();
        }

        TestUtils.waitForExecutor(telemetry);

        // No ping queued so far
        verify(telemetry, never()).queuePing(TelemetryMobileEventPingBuilder.TYPE);

        // Queue event number 500
        TelemetryEvent.create("category", "method", "object", "500").queue();

        TestUtils.waitForExecutor(telemetry);

        verify(telemetry).queuePing(TelemetryMobileEventPingBuilder.TYPE);
    }

    @Test
    @Deprecated // If you change this test, change the one above it.
    public void testPingIsQueuedIfEventLimitIsReachedLegacyPingType() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        // This test assumes the maximum number of events is 500
        assertEquals(500, configuration.getMaximumNumberOfEventsPerPing());

        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final TelemetryEventPingBuilder pingBuilder = new TelemetryEventPingBuilder(configuration);
        final Telemetry telemetry = spy(new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder));

        TelemetryHolder.set(telemetry);

        for (int i = 0; i < 499; i++) {
            TelemetryEvent.create("category", "method", "object", String.valueOf(i)).queue();
        }

        TestUtils.waitForExecutor(telemetry);

        // No ping queued so far
        verify(telemetry, never()).queuePing(TelemetryEventPingBuilder.TYPE);

        // Queue event number 500
        TelemetryEvent.create("category", "method", "object", "500").queue();

        TestUtils.waitForExecutor(telemetry);

        verify(telemetry).queuePing(TelemetryEventPingBuilder.TYPE);
    }

    private void assertJobIsScheduled() {
        final Context context = RuntimeEnvironment.application;
        final JobScheduler scheduler = (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);

        final List<JobInfo> jobs = scheduler.getAllPendingJobs();
        assertEquals(1, jobs.size());

        final JobInfo job = jobs.get(0);
        assertNotNull(job);
    }

    private void executePendingJob(String pingType) {
        final TelemetryJobService service = spy(Robolectric.buildService(TelemetryJobService.class)
                .create()
                .get());

        final PersistableBundle bundle = mock(PersistableBundle.class);
        doReturn(pingType).when(bundle).getString(anyString());

        final JobParameters parameters = mock(JobParameters.class);
        doReturn(bundle).when(parameters).getExtras();

        AsyncTask task = mock(AsyncTask.class);

        service.uploadPingsInBackground(task, parameters);

        verify(service).jobFinished(parameters, false);
    }

    @After
    public void tearDown() {
        TelemetryHolder.set(null);
    }
}
