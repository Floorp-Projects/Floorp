/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.media;

import android.content.Context;
import android.content.Intent;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
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
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestMediaControlService {

    private MediaControlService mSpyService;
    private AudioFocusAgent mSpyAudioAgent;
    private Context mMockContext;
    private Tab mMockTab;

    @Before
    public void setUp() {
        MediaControlService service = Robolectric.buildService(MediaControlService.class).get();
        mSpyService = spy(service);
        mSpyAudioAgent = spy(AudioFocusAgent.getInstance());
        mMockContext = mock(Context.class);
        mMockTab = mock(Tab.class);
        // We should use White-box as less as possible. But this is not avoidable so far.
        Whitebox.setInternalState(mSpyService, "mInitialize", true);
        Whitebox.setInternalState(mSpyAudioAgent,"mContext", mMockContext);
    }

    @Test
    public void testTabPlayingMedia() throws Exception {
        // If the tab is playing media and we got another MEDIA_PLAYING_CHANGE,
        // we should notify the service that its state should be PLAYING.
        Whitebox.setInternalState(mSpyAudioAgent, "mTabReference", new WeakReference<>(mMockTab));
        doReturn(true).when(mMockTab).isMediaPlaying();

        mSpyAudioAgent.onTabChanged(mMockTab, Tabs.TabEvents.MEDIA_PLAYING_CHANGE, "");
        ArgumentCaptor<Intent> serviceIntent = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).startService(serviceIntent.capture());
        Assert.assertEquals(MediaControlService.ACTION_TAB_STATE_PLAYING, serviceIntent.getValue().getAction());
    }

    @Test
    public void testTabNotPlayingMedia() throws Exception {
        // If the tab is not playing media and we got another MEDIA_PLAYING_CHANGE,
        // we should notify the service that its state should be STOPPED.
        Whitebox.setInternalState(mSpyAudioAgent, "mTabReference", new WeakReference<>(mMockTab));
        doReturn(false).when(mMockTab).isMediaPlaying();

        mSpyAudioAgent.onTabChanged(mMockTab, Tabs.TabEvents.MEDIA_PLAYING_CHANGE, "");
        ArgumentCaptor<Intent> serviceIntent = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).startService(serviceIntent.capture());
        Assert.assertEquals(MediaControlService.ACTION_TAB_STATE_STOPPED, serviceIntent.getValue().getAction());
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
