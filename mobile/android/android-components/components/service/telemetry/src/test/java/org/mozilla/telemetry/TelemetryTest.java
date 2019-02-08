/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Build;
import android.os.PersistableBundle;
import android.preference.PreferenceManager;
import kotlin.Unit;
import kotlin.jvm.functions.Function0;
import mozilla.components.concept.fetch.Client;
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient;
import mozilla.components.lib.fetch.okhttp.OkHttpClient;
import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;
import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement;
import org.mozilla.telemetry.measurement.ExperimentsMapMeasurement;
import org.mozilla.telemetry.measurement.SearchesMeasurement;
import org.mozilla.telemetry.measurement.SessionDurationMeasurement;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryMobileEventPingBuilder;
import org.mozilla.telemetry.ping.TelemetryPocketEventPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.schedule.jobscheduler.JobSchedulerTelemetryScheduler;
import org.mozilla.telemetry.schedule.jobscheduler.TelemetryJobService;
import org.mozilla.telemetry.serialize.JSONPingSerializer;
import org.mozilla.telemetry.serialize.TelemetryPingSerializer;
import org.mozilla.telemetry.storage.FileTelemetryStorage;
import org.mozilla.telemetry.storage.TelemetryStorage;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.*;

import static org.junit.Assert.*;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.*;

@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(minSdk = Build.VERSION_CODES.M)
public class TelemetryTest {
    private static final String TEST_USER_AGENT = "Test/42.0.23";
    private static final String TEST_SETTING_1 = "test-setting-1";
    private static final String TEST_SETTING_2 = "test-setting-2";

    @ParameterizedRobolectricTestRunner.Parameters(name = "{1}")
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {
            { new HttpURLConnectionClient(), "HttpUrlConnection" },
            { new OkHttpClient(), "OkHttp" }
        });
    }

    private final Client httpClient;

    public TelemetryTest(Client client, String name) {
        this.httpClient = client;
    }

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

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
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

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
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

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
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

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
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
    public void testPocketPingIntegration() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setAppName("TelemetryTest")
                .setAppVersion("13.1.3")
                .setUpdateChannel("test")
                .setBuildId("789")
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort())
                .setUserAgent(TEST_USER_AGENT);

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final TelemetryPocketEventPingBuilder pingBuilder = spy(new TelemetryPocketEventPingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(pingBuilder).generateDocumentId();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder);
        TelemetryHolder.set(telemetry);

        {
            final TelemetryPocketEventPingBuilder pocketPingBuilder =
                    (TelemetryPocketEventPingBuilder) telemetry.getPingBuilder(TelemetryPocketEventPingBuilder.TYPE);

            assertNotNull(pocketPingBuilder);

            pocketPingBuilder.getEventsMeasurement()
                    .add(TelemetryEvent.create("action", "type", "search_bar"))
                    .add(TelemetryEvent.create("action", "type_query", "search_bar"))
                    .add(TelemetryEvent.create("action", "click", "erase_button"));
        }

        telemetry.queuePing(TelemetryPocketEventPingBuilder.TYPE);
        telemetry.scheduleUpload();

        TestUtils.waitForExecutor(telemetry);

        assertEquals(1, storage.countStoredPings(TelemetryPocketEventPingBuilder.TYPE));

        assertJobIsScheduled();
        executePendingJob(TelemetryPocketEventPingBuilder.TYPE);

        verify(client).uploadPing(eq(configuration), anyString(), anyString());

        assertEquals(0, storage.countStoredPings(TelemetryPocketEventPingBuilder.TYPE));

        final RecordedRequest request = server.takeRequest();
        assertEquals("POST /submit/pocket/fire-tv-events/1/ffffffff-0000-0000-ffff-ffffffffffff HTTP/1.1", request.getRequestLine());
        assertEquals("application/json; charset=utf-8", request.getHeader("Content-Type"));
        assertEquals(TEST_USER_AGENT, request.getHeader("User-Agent"));
        assertNotNull(request.getHeader("Date"));

        final JSONObject object = new JSONObject(request.getBody().readUtf8());

        assertFalse(object.has("clientId")); //Make sure pocket ping doesn't include client-id
        assertTrue(object.has("pocketId"));
        assertTrue(object.has("v"));
        assertTrue(object.has("seq"));
        assertTrue(object.has("locale"));
        assertTrue(object.has("os"));
        assertTrue(object.has("device"));
        assertTrue(object.has("created"));
        assertTrue(object.has("tz"));
        assertTrue(object.has("events"));

        final JSONArray events = object.getJSONArray("events");
        assertEquals(3, events.length());

        server.shutdown();
    }

    @Test
    public void testPocketEventsAreQueuedToPocketPing() throws Exception {
        final MockWebServer server = new MockWebServer();
        server.enqueue(new MockResponse().setBody("OK"));

        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setAppName("TelemetryTest")
                .setAppVersion("13.1.3")
                .setUpdateChannel("test")
                .setBuildId("789")
                .setServerEndpoint("http://" + server.getHostName() + ":" + server.getPort())
                .setUserAgent(TEST_USER_AGENT);

        final TelemetryPingSerializer serializer = new JSONPingSerializer();
        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);

        final TelemetryClient client = spy(new TelemetryClient(httpClient));
        final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

        final TelemetryPocketEventPingBuilder pocketPingBuilder = spy(new TelemetryPocketEventPingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(pocketPingBuilder).generateDocumentId();

        final TelemetryMobileEventPingBuilder eventPingBuilder = spy(new TelemetryMobileEventPingBuilder(configuration));
        doReturn("ffffffff-0000-0000-ffff-ffffffffffff").when(eventPingBuilder).generateDocumentId();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pocketPingBuilder).addPingBuilder(eventPingBuilder);

        TelemetryHolder.set(telemetry);

        {
            final TelemetryMobileEventPingBuilder moPingBuilder =
                    (TelemetryMobileEventPingBuilder) telemetry.getPingBuilder(TelemetryMobileEventPingBuilder.TYPE);

            assertNotNull(moPingBuilder);

            moPingBuilder.getEventsMeasurement()
                    .add(TelemetryEvent.create("action", "type", "search_bar"))
                    .add(TelemetryEvent.create("action", "type_query", "search_bar"))
                    .add(TelemetryEvent.create("action", "click", "erase_button"));

        }

        {
            final TelemetryPocketEventPingBuilder poPingBuilder =
                    (TelemetryPocketEventPingBuilder) telemetry.getPingBuilder(TelemetryPocketEventPingBuilder.TYPE);

            assertNotNull(poPingBuilder);

            poPingBuilder.getEventsMeasurement()
                    .add(TelemetryEvent.create("action", "click", "video_id"))
                    .add(TelemetryEvent.create("action", "impression", "video_id"))
                    .add(TelemetryEvent.create("action", "click", "video_id"));
        }

        telemetry.queuePing(TelemetryPocketEventPingBuilder.TYPE);
        telemetry.queuePing(TelemetryMobileEventPingBuilder.TYPE);
        telemetry.scheduleUpload();

        TestUtils.waitForExecutor(telemetry);

        assertJobIsScheduled();

        executePendingJob(TelemetryPocketEventPingBuilder.TYPE);

        final RecordedRequest pocketRequest = server.takeRequest();

        final JSONObject pocketObject = new JSONObject(pocketRequest.getBody().readUtf8());

        final JSONArray pocketEvents = pocketObject.getJSONArray("events");
        assertEquals(3, pocketEvents.length());

        final JSONArray event1 = pocketEvents.getJSONArray(0);
        assertEquals(4, event1.length());
        assertTrue(event1.getLong(0) >= 0);
        assertEquals("action", event1.getString(1));
        assertEquals("click", event1.getString(2));
        assertEquals("video_id", event1.getString(3));

        final JSONArray event2 = pocketEvents.getJSONArray(1);
        assertEquals(4, event2.length());
        assertTrue(event2.getLong(0) >= 0);
        assertEquals("action", event2.getString(1));
        assertEquals("impression", event2.getString(2));
        assertEquals("video_id", event2.getString(3));

        final JSONArray event3 = pocketEvents.getJSONArray(2);
        assertEquals(4, event3.length());
        assertTrue(event3.getLong(0) >= 0);
        assertEquals("action", event3.getString(1));
        assertEquals("click", event3.getString(2));
        assertEquals("video_id", event3.getString(3));

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

    @Test
    public void testClientIdIsReturned() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler);
        final String clientId = telemetry.getClientId();

        assertNotNull(clientId);

        assertTrue(clientId.matches("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"));

        final UUID uuid = UUID.fromString(clientId); // Should throw if invalid
        assertEquals(4, uuid.version());

        // Subsequent calls return the same client id.
        assertEquals(clientId, telemetry.getClientId());

        // Creating a new telemetry object will still return the same client id
        final Telemetry otherTelemetry = new Telemetry(
                new TelemetryConfiguration(RuntimeEnvironment.application),
                mock(TelemetryStorage.class),
                mock(TelemetryClient.class),
                mock(TelemetryScheduler.class));

        assertEquals(clientId, telemetry.getClientId());
    }

    @Test
    public void testExperimentsAreForwardedToFocusEventPingMeasurement() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final ExperimentsMapMeasurement measurement = mock(ExperimentsMapMeasurement.class);
        final TelemetryEventPingBuilder builder = spy(new TelemetryEventPingBuilder(configuration));
        doReturn(measurement).when(builder).getExperimentsMapMeasurement();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(builder);

        final Map<String, Boolean> experiments = new HashMap<>();
        experiments.put("use-gecko", true);
        experiments.put("use-homescreen-tips", false);

        telemetry.recordExperiments(experiments);

        verify(measurement).setExperiments(experiments);
    }

    @Test
    public void testExperimentsAreForwardedToMobileEventPingMeasurement() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);

        final ExperimentsMapMeasurement measurement = mock(ExperimentsMapMeasurement.class);
        final TelemetryMobileEventPingBuilder builder = spy(new TelemetryMobileEventPingBuilder(configuration));
        doReturn(measurement).when(builder).getExperimentsMapMeasurement();

        final Telemetry telemetry = new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(builder);

        final Map<String, Boolean> experiments = new HashMap<>();
        experiments.put("use-gecko", true);
        experiments.put("use-homescreen-tips", false);

        telemetry.recordExperiments(experiments);

        verify(measurement).setExperiments(experiments);
    }

    @Test(expected = IllegalStateException.class)
    public void testFailedSessionEndThrows() {
        Telemetry telemetry = createFailOnSessionEndTelemetry();
        when(telemetry.recordSessionEnd()).thenThrow(new IllegalStateException());
    }

    @Test
    public void testFailedSessionEndInvokesCallback() {
        Telemetry telemetry = createFailOnSessionEndTelemetry();
        Function0<Unit> onFailure = spy(new Function0<Unit>() {
            @Override
            public Unit invoke() {
                return null;
            }
        });

        telemetry.recordSessionEnd(onFailure);
        verify(onFailure, times(1)).invoke();
    }

    private Telemetry createFailOnSessionEndTelemetry() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final TelemetryStorage storage = mock(TelemetryStorage.class);
        final TelemetryClient client = mock(TelemetryClient.class);
        final TelemetryScheduler scheduler = mock(TelemetryScheduler.class);
        final SessionDurationMeasurement sessionDurationMeasurement = new SessionDurationMeasurement(configuration) {
            @Override
            public synchronized boolean recordSessionEnd() {
                return false;
            }
        };
        final TelemetryCorePingBuilder pingBuilder = new TelemetryCorePingBuilder(configuration) {
            @Override
            public SessionDurationMeasurement getSessionDurationMeasurement() {
                return sessionDurationMeasurement;
            }
        };


        return new Telemetry(configuration, storage, client, scheduler)
                .addPingBuilder(pingBuilder);
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
