/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.media.AudioFocusAgent.State;

import android.media.AudioManager;

public class testAudioFocus extends MediaPlaybackTest {
    public void testAudioFocus() {
        info("- wait for gecko ready -");
        blockForGeckoReady();

        info("- run test : testBasicAbility -");
        testBasicAbility();

        info("- run test : testAudioFocusChanged -");
        testAudioFocusChanged();

        info("- run test : testSwitchTab -");
        testSwitchTab();

        info("- run test : testAdjustMediaVolumeOrMuted -");
        testAdjustMediaVolumeOrMuted();
    }

    private void testBasicAbility() {
        info("- check audio focus in the beginning -");
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS,
                     "Should not own audio focus.");

        info("- request audio focus -");
        requestAudioFocus();
        mAsserter.ok(true,
                     "Check audio focus state",
                     "Should own audio focus.");

        info("- simulate losing audio focus transiently -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS_TRANSIENT,
                     "Should lose audio focus.");

        info("- simulate gaining audio focus again -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.OWN_FOCUS,
                     "Should own audio focus.");

        info("- simulate losing audio focus and can duck -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS_TRANSIENT_CAN_DUCK,
                     "Should lose audio focus and can duck.");

        info("- simulate gaining audio focus again -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.OWN_FOCUS,
                     "Should own audio focus.");

        info("- simulate losing audio focus -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS);
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS,
                     "Should lose audio focus.");

        info("- request audio focus -");
        requestAudioFocus();
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.OWN_FOCUS,
                     "Should own audio focus.");

        info("- abandon audio focus -");
        getAudioFocusAgent().notifyStoppedPlaying();
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS,
                     "Should lose audio focus.");
    }

    private void testAudioFocusChanged() {
        info("- check audio focus in the beginning -");
        mAsserter.is(getAudioFocusAgent().getAudioFocusState(),
                     State.LOST_FOCUS,
                     "Should not request audio focus before media starts.");

        info("- load URL with looping audio file -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- wait tab becomes audible -");
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- simulate losing audio focus transiently -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        checkTabAudioPlayingState(tab, false /* non-audible */);
        checkAudioFocusStateAfterChanged(false);

        info("- simulate gaining audio focus again -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- simulate losing audio focus -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS);
        checkTabAudioPlayingState(tab, false /* non-audible */);
        checkAudioFocusStateAfterChanged(false);

        info("- close tab -");
        closeAllTabs();
    }

    private void testSwitchTab() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- wait tab becomes audible -");
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- switch to the another tab -");
        final String BLANK_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        addTab(BLANK_URL);

        info("- should still own the audio focus -");
        checkAudioFocusStateAfterChanged(true);

        info("- close tab -");
        closeAllTabs();
    }

   /**
     * Audio focus should only be requested when media is audible.
     */
    private void testAdjustMediaVolumeOrMuted() {
        info("- create JSBridge -");
        createJSBridge();

        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_JS_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- play media -");
        getJS().syncCall("play_audio");

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- wait tab becomes audible -");
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- change media's volume to 0.0 -");
        getJS().syncCall("adjust_audio_volume", 0.0);
        checkTabAudioPlayingState(tab, false /* non-audible */);
        checkAudioFocusStateAfterChanged(false);

        info("- change media's volume to 1.0 -");
        getJS().syncCall("adjust_audio_volume", 1.0);
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- mute media -");
        getJS().syncCall("adjust_audio_muted", true);
        checkTabAudioPlayingState(tab, false /* non-audible */);
        checkAudioFocusStateAfterChanged(false);

        info("- unmute media -");
        getJS().syncCall("adjust_audio_muted", false);
        checkTabAudioPlayingState(tab, true /* audible */);
        checkAudioFocusStateAfterChanged(true);

        info("- pause media -");
        getJS().syncCall("pause_audio");
        checkTabAudioPlayingState(tab, false /* non-audible */);
        checkAudioFocusStateAfterChanged(false);

        info("- close tab -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }
}
