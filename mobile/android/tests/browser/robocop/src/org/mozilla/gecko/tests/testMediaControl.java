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
     * Media control should always be displayed even the media becomes non-audible.
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
}
