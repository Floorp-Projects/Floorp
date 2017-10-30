/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session;

import android.content.Context;
import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class NotificationSessionObserverTest {
    @Test
    public void testEmptySession() {
        final Context context = mock(Context.class);

        final NotificationSessionObserver observer = new NotificationSessionObserver(context);
        observer.onChanged(Collections.<Session>emptyList());

        verify(context).stopService(anyIntent());
        verify(context, never()).startService(anyIntent());
        verify(context, never()).startForegroundService(anyIntent());
    }

    @Test
    public void testNullSession() {
        final Context context = mock(Context.class);

        final NotificationSessionObserver observer = new NotificationSessionObserver(context);
        observer.onChanged(null);

        verify(context, never()).stopService(anyIntent());
        verify(context, never()).startService(anyIntent());
        verify(context, never()).startForegroundService(anyIntent());
    }

    @Test
    public void testSingleSession() {
        final Context context = mock(Context.class);

        final List<Session> sessions = Collections.singletonList(mock(Session.class));

        final NotificationSessionObserver observer = new NotificationSessionObserver(context);
        observer.onChanged(sessions);

        verify(context, never()).stopService(anyIntent());
        verify(context).startForegroundService(anyIntent());
    }

    @Test
    public void testMultipleSessions() {
        final Context context = mock(Context.class);

        final List<Session> sessions = Arrays.asList(
                mock(Session.class),
                mock(Session.class),
                mock(Session.class));

        final NotificationSessionObserver observer = new NotificationSessionObserver(context);
        observer.onChanged(sessions);

        verify(context, never()).stopService(anyIntent());
        verify(context).startForegroundService(anyIntent());
    }

    private Intent anyIntent() {
        return any(Intent.class);
    }
}