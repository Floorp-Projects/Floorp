/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.media;

import android.content.Intent;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.internal.util.reflection.Whitebox;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.media.MediaControlService.State;
import org.robolectric.Robolectric;

import java.lang.ref.WeakReference;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

@RunWith(TestRunner.class)
public class TestMediaControlService {

    private MediaControlService mSpyService;
    private Tab mMockTab;

    @Before
    public void setUp() {
        MediaControlService service = Robolectric.buildService(MediaControlService.class).get();
        mSpyService = spy(service);
        mMockTab = mock(Tab.class);
        // We should use White-box as less as possible. But this is not avoidable so far.
        Whitebox.setInternalState(mSpyService, "mInitialize", true);
    }

    @Test
    public void testTabPlayingMedia() throws Exception {
        // If tab is playing media and got another MEDIA_PLAYING_CHANGE
        // state should be PLAYING
        Whitebox.setInternalState(mSpyService, "mTabReference", new WeakReference<>(mMockTab));
        doReturn(true).when(mMockTab).isMediaPlaying();

        mSpyService.onTabChanged(mMockTab, Tabs.TabEvents.MEDIA_PLAYING_CHANGE, "");
        State state = (State) Whitebox.getInternalState(mSpyService, "mMediaState");
        Assert.assertEquals(state, State.PLAYING);
    }

    @Test
    public void testTabNotPlayingMedia() throws Exception {
        // If tab is not playing media and got another MEDIA_PLAYING_CHANGE
        // state should be STOPPED
        Whitebox.setInternalState(mSpyService, "mTabReference", new WeakReference<>(mMockTab));
        doReturn(false).when(mMockTab).isMediaPlaying();

        mSpyService.onTabChanged(mMockTab, Tabs.TabEvents.MEDIA_PLAYING_CHANGE, "");
        State state = (State) Whitebox.getInternalState(mSpyService, "mMediaState");
        Assert.assertEquals(state, State.STOPPED);
    }

    @Test
    public void testIntentForPlayingState() throws Exception {
        // For PLAYING state, should create an PAUSE intent for notification
        Intent intent = mSpyService.createIntentUponState(State.PLAYING);
        Assert.assertEquals(intent.getAction(), MediaControlService.ACTION_PAUSE);
    }

    @Test
    public void testIntentForPausedState() throws Exception {
        // For PAUSED state, should create an RESUME intent for notification
        Intent intent = mSpyService.createIntentUponState(State.PAUSED);
        Assert.assertEquals(intent.getAction(), MediaControlService.ACTION_RESUME);
    }
}
