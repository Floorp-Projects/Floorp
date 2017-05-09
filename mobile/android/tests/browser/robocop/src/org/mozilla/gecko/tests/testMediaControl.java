/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.media.MediaControlService;

import android.media.AudioManager;

public class testMediaControl extends MediaPlaybackTest {
    public void testMediaControl() {
        info("- ensure the test is running on correct Android version -");
        checkAndroidVersionForMediaControlTest();

        info("- wait for gecko ready -");
        blockForGeckoReady();

        info("- run test : testBasicBehaviors -");
        testBasicBehaviors();

        info("- run test : testNavigateOutThePage -");
        testNavigateOutThePage();

        info("- run test : testAudioFocusChanged -");
        testAudioFocusChanged();

        info("- run test : testSwitchTab -");
        testSwitchTab();

        info("- run test : testCloseTab -");
        testCloseTab();

        info("- run test : testResumeMediaFromPage -");
        testResumeMediaFromPage();

        info("- run test : testAdjustMediaVolumeOrMuted -");
        testAdjustMediaVolumeOrMuted();

        info("- run test : testMediaWithSilentAudioTrack -");
        testMediaWithSilentAudioTrack();

        info("- run test : testMediaWithoutAudioTrack -");
        testMediaWithoutAudioTrack();

        info("- run test : testAudioCompetingForMediaWithSilentAudioTrack -");
        testAudioCompetingForMediaWithSilentAudioTrack();

        info("- run test : testAudioCompetingForMediaWithoutAudioTrack -");
        testAudioCompetingForMediaWithoutAudioTrack();
    }

    private void testBasicBehaviors() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- simulate media control pause -");
        notifyMediaControlService(MediaControlService.ACTION_PAUSE);
        checkIfMediaPlayingSuccess(false /* paused */);

        info("- simulate media control resume -");
        notifyMediaControlService(MediaControlService.ACTION_RESUME);
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- simulate media control stop -");
        notifyMediaControlService(MediaControlService.ACTION_STOP);
        checkIfMediaPlayingSuccess(false /* paused */, true /* clear notification */);

        info("- close tab -");
        closeAllTabs();
    }

    private void testNavigateOutThePage() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- navigate out the present page -");
        final String BLANK_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        loadUrlAndWait(BLANK_URL);
        checkIfMediaPlayingSuccess(false /* paused */, true /* clear notification */);

        info("- close tab -");
        closeAllTabs();

        info("- run next test : testAudioFocusChanged -");
        testAudioFocusChanged();
    }

    private void testAudioFocusChanged() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- simulate lose audio focus transiently -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
        checkIfMediaPlayingSuccess(false /* paused */);

        info("- simulate gain audio focus again -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- simulate lose audio focus -");
        getAudioFocusAgent().changeAudioFocus(AudioManager.AUDIOFOCUS_LOSS);
        checkIfMediaPlayingSuccess(false /* paused */);

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

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- switch to the another tab -");
        final String BLANK_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        addTab(BLANK_URL);

        info("- the media control shouldn't be changed and display the info of audible tab -");
        checkMediaNotificationStates(tab, true /* playing */);

        info("- close tab -");
        closeAllTabs();
    }

    private void testCloseTab() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- close audible tab -");
        Tabs.getInstance().closeTab(tab);

        info("- media control should disappear -");
        waitUntilNotificationUIChanged();
        checkIfMediaNotificationBeCleared();

        info("- close tab -");
        closeAllTabs();
    }

   /**
     * Media control and audio focus should be changed as well when user resume
     * media from page, instead of from media control.
     */
    private void testResumeMediaFromPage() {
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
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- simulate media control pause -");
        notifyMediaControlService(MediaControlService.ACTION_PAUSE);
        checkIfMediaPlayingSuccess(false /* paused */);

        info("- resume media from page -");
        getJS().syncCall("play_audio");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- pause media from page -");
        getJS().syncCall("pause_audio");
        checkIfMediaPlayingSuccess(false /* paused */, true /* clear notification */);

        info("- close tab -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }

    /**
     * There are three situations that media would be non-audible,
     * (1) media is muted/set volume to ZERO
     * (2) media has silent audio track
     * (3) media doesn't have audio track
     * We would show the media control for (1) and (2), but won't show it for (3).
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
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- change media's volume to 0.0 -");
        getJS().syncCall("adjust_audio_volume", 0.0);
        checkMediaNotificationStates(tab, true);

        info("- change media's volume to 1.0 -");
        getJS().syncCall("adjust_audio_volume", 1.0);
        checkMediaNotificationStates(tab, true);

        info("- mute media -");
        getJS().syncCall("adjust_audio_muted", true);
        checkMediaNotificationStates(tab, true);

        info("- unmute media -");
        getJS().syncCall("adjust_audio_muted", false);
        checkMediaNotificationStates(tab, true);

        info("- pause media -");
        getJS().syncCall("pause_audio");
        checkIfMediaPlayingSuccess(false /* paused */, true /* clear notification */);

        info("- close tab -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }

    private void testMediaWithSilentAudioTrack() {
        info("- create JSBridge -");
        createJSBridge();

        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_JS_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- play media with silent audio track -");
        getJS().syncCall("play_media_with_silent_audio_track");

        info("- wait until media starts playing -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(tab);

        info("- media control should be displayed -");
        checkTabMediaPlayingState(tab, true);
        checkMediaNotificationStatesAfterChanged(tab,
                                                 true /* playing */);

        info("- pause media with silent audio track -");
        getJS().syncCall("pause_media_with_silent_audio_track");

        info("- media control should disappear -");
        checkMediaNotificationStatesAfterChanged(tab,
                                                 false /* non-playing */,
                                                 true /* clear control */);
        info("- close tab -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }

    private void testMediaWithoutAudioTrack() {
        info("- create JSBridge -");
        createJSBridge();

        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_JS_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- play media -");
        getJS().syncCall("play_media_without_audio_track");

        // We can't know whether it starts or not for media without audio track,
        // because we won't dispatch Tab:MediaPlaybackChange event for this kind
        // of media. So we just check the state multiple times to make sure we
        // don't show the media control.
        info("- should not show control -");
        final Tab tab = Tabs.getInstance().getSelectedTab();
        checkTabMediaPlayingState(tab, false);
        checkIfMediaNotificationBeCleared();

        info("- should not show control -");
        checkTabMediaPlayingState(tab, false);
        checkIfMediaNotificationBeCleared();

        info("- should not show control -");
        checkTabMediaPlayingState(tab, false);
        checkIfMediaNotificationBeCleared();

        info("- should not show control -");
        checkTabMediaPlayingState(tab, false);
        checkIfMediaNotificationBeCleared();

        info("- should not show control -");
        checkTabMediaPlayingState(tab, false);
        checkIfMediaNotificationBeCleared();

        info("- pause media -");
        getJS().syncCall("pause_media_without_audio_track");

        info("- close tab -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }

    /**
     * There are three situations that media would be non-audible,
     * (1) media is muted/set volume to ZERO
     * (2) media has silent audio track
     * (3) media doesn't have audio track
     * (1) and (2) would involve in the audio competing within tabs, but (3)
     * won't, because (3) are more likely GIF-like video and we don't want it
     * interrupts background audio playback.
     */
    private void testAudioCompetingForMediaWithSilentAudioTrack() {
        info("- create JSBridge -");
        createJSBridge();

        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab audibleTab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(audibleTab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- switch to the another tab -");
        final String MEDIA_JS_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_JS_URL);
        addTab(MEDIA_JS_URL);

        info("- play silent media from new tab -");
        getJS().syncCall("play_media_with_silent_audio_track");

        info("- wait until silent media starts playing -");
        Tab silentTab = Tabs.getInstance().getFirstTabForUrl(MEDIA_JS_URL);
        waitUntilTabMediaStarted(silentTab);

        info("- audible tab should be stopped because of audio competing -");
        checkTabMediaPlayingState(audibleTab, false);
        checkTabAudioPlayingState(audibleTab, false);

        info("- should show media control for silent tab -");
        checkMediaNotificationStatesAfterChanged(silentTab,
                                                 true /* playing */);

        info("- pause silent media -");
        getJS().syncCall("play_media_with_silent_audio_track");

        info("- close tabs -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }

    private void testAudioCompetingForMediaWithoutAudioTrack() {
        info("- create JSBridge -");
        createJSBridge();

        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- wait until media starts playing -");
        final Tab audibleTab = Tabs.getInstance().getSelectedTab();
        waitUntilTabMediaStarted(audibleTab);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- switch to the another tab -");
        final String MEDIA_JS_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_JS_URL);
        addTab(MEDIA_JS_URL);

        info("- play silent media from new tab -");
        getJS().syncCall("play_media_without_audio_track");

        // Same with testMediaWithoutAudioTrack, we can't know when the media
        // has started, so just check the states multiple times.
        info("- audible tab should still be playing and show media control -");
        checkTabMediaPlayingState(audibleTab, true);
        checkTabAudioPlayingState(audibleTab, true);
        checkMediaNotificationStates(audibleTab, true /* playing */);

        info("- audible tab should still be playing and show media control -");
        checkTabMediaPlayingState(audibleTab, true);
        checkTabAudioPlayingState(audibleTab, true);
        checkMediaNotificationStates(audibleTab, true /* playing */);

        info("- audible tab should still be playing and show media control -");
        checkTabMediaPlayingState(audibleTab, true);
        checkTabAudioPlayingState(audibleTab, true);
        checkMediaNotificationStates(audibleTab, true /* playing */);

        info("- pause sielent media -");
        getJS().syncCall("pause_media_without_audio_track");

        info("- close tabs -");
        closeAllTabs();

        info("- destroy JSBridge -");
        destroyJSBridge();
    }
}
