/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.telemetry.measurements;

import android.content.Context;
import android.content.SharedPreferences;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.telemetry.measurements.SessionMeasurements.SessionMeasurementsContainer;
import org.robolectric.RuntimeEnvironment;

import java.util.concurrent.TimeUnit;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

/**
 * Tests the session measurements class.
 */
@RunWith(TestRunner.class)
public class TestSessionMeasurements {

    private SessionMeasurements testMeasurements;
    private SharedPreferences sharedPrefs;
    private Context context;

    @Before
    public void setUp() throws Exception {
        testMeasurements = spy(SessionMeasurements.class);
        sharedPrefs = RuntimeEnvironment.application.getSharedPreferences(
                TestSessionMeasurements.class.getSimpleName(), Context.MODE_PRIVATE);
        doReturn(sharedPrefs).when(testMeasurements).getSharedPreferences(any(Context.class));

        context = RuntimeEnvironment.application;
    }

    private void assertSessionCount(final String postfix, final int expectedSessionCount) {
        final int actual = sharedPrefs.getInt(SessionMeasurements.PREF_SESSION_COUNT, -1);
        assertEquals("Expected number of sessions occurred " + postfix, expectedSessionCount, actual);
    }

    private void assertSessionDuration(final String postfix, final long expectedSessionDuration) {
        final long actual = sharedPrefs.getLong(SessionMeasurements.PREF_SESSION_DURATION, -1);
        assertEquals("Expected session duration received " + postfix, expectedSessionDuration, actual);
    }

    private void mockGetSystemTimeNanosToReturn(final long value) {
        doReturn(value).when(testMeasurements).getSystemTimeNano();
    }

    @Test
    public void testRecordSessionStartAndEndCalledOnce() throws Exception {
        final long expectedElapsedSeconds = 4;
        mockGetSystemTimeNanosToReturn(0);
        testMeasurements.recordSessionStart();
        mockGetSystemTimeNanosToReturn(TimeUnit.SECONDS.toNanos(expectedElapsedSeconds));
        testMeasurements.recordSessionEnd(context);

        final String postfix = "after recordSessionStart/End called once";
        assertSessionCount(postfix, 1);
        assertSessionDuration(postfix, expectedElapsedSeconds);
    }

    @Test
    public void testRecordSessionStartAndEndCalledTwice() throws Exception {
        final long expectedElapsedSeconds = 100;
        mockGetSystemTimeNanosToReturn(0L);
        for (int i = 1; i <= 2; ++i) {
            testMeasurements.recordSessionStart();
            mockGetSystemTimeNanosToReturn(TimeUnit.SECONDS.toNanos((expectedElapsedSeconds / 2) * i));
            testMeasurements.recordSessionEnd(context);
        }

        final String postfix = "after recordSessionStart/End called twice";
        assertSessionCount(postfix, 2);
        assertSessionDuration(postfix, expectedElapsedSeconds);
    }

    @Test(expected = IllegalStateException.class)
    public void testRecordSessionStartThrowsIfSessionAlreadyStarted() throws Exception {
        // First call will start the session, next expected to throw.
        for (int i = 0; i < 2; ++i) {
            testMeasurements.recordSessionStart();
        }
    }

    @Test(expected = IllegalStateException.class)
    public void testRecordSessionEndThrowsIfCalledBeforeSessionStarted() {
        testMeasurements.recordSessionEnd(context);
    }

    @Test // assumes the underlying format in SessionMeasurements
    public void testGetAndResetSessionMeasurementsReturnsSetData() throws Exception {
        final int expectedSessionCount = 42;
        final long expectedSessionDuration = 1234567890;
        sharedPrefs.edit()
                .putInt(SessionMeasurements.PREF_SESSION_COUNT, expectedSessionCount)
                .putLong(SessionMeasurements.PREF_SESSION_DURATION, expectedSessionDuration)
                .apply();

        final SessionMeasurementsContainer actual = testMeasurements.getAndResetSessionMeasurements(context);
        assertEquals("Returned session count matches expected", expectedSessionCount, actual.sessionCount);
        assertEquals("Returned session duration matches expected", expectedSessionDuration, actual.elapsedSeconds);
    }

    @Test
    public void testGetAndResetSessionMeasurementsResetsData() throws Exception {
        sharedPrefs.edit()
                .putInt(SessionMeasurements.PREF_SESSION_COUNT, 10)
                .putLong(SessionMeasurements.PREF_SESSION_DURATION, 10)
                .apply();

        testMeasurements.getAndResetSessionMeasurements(context);
        final String postfix = "is reset after retrieval";
        assertSessionCount(postfix, 0);
        assertSessionDuration(postfix, 0);
    }
}