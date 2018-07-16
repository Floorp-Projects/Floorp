/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.media;

import android.content.Context;
import android.content.Intent;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.internal.util.reflection.Whitebox;
import org.robolectric.RobolectricTestRunner;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

@RunWith(RobolectricTestRunner.class)
public class TestGeckoMediaControlAgent {

    private GeckoMediaControlAgent mSpyMediaAgent;

    @Before
    public void setUp() {
        Context mMockContext = mock(Context.class);
        mSpyMediaAgent = spy(GeckoMediaControlAgent.getInstance());
        // We should use White-box as less as possible. But this is not avoidable so far.
        Whitebox.setInternalState(mSpyMediaAgent, "mContext", mMockContext);
    }

    @Test
    public void testIntentForPlayingState() throws Exception {
        // For PLAYING state, should create an PAUSE intent for notification
        Intent intent = mSpyMediaAgent.createIntentUponState(GeckoMediaControlAgent.State.PLAYING);
        Assert.assertEquals(intent.getAction(), GeckoMediaControlAgent.ACTION_PAUSE);
    }

    @Test
    public void testIntentForPausedState() throws Exception {
        // For PAUSED state, should create an RESUME intent for notification
        Intent intent = mSpyMediaAgent.createIntentUponState(GeckoMediaControlAgent.State.PAUSED);
        Assert.assertEquals(intent.getAction(), GeckoMediaControlAgent.ACTION_RESUME);
    }
}
