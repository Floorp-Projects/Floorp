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
    }

    private void testBasicBehaviors() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

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

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- switch to the another tab -");
        final String BLANK_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        addTab(BLANK_URL);

        info("- the media control shouldn't be changed and display the info of audible tab -");
        final Tab tab = Tabs.getInstance().getFirstTabForUrl(MEDIA_URL);
        checkMediaNotificationStates(true /* playing */, tab);

        info("- close tab -");
        closeAllTabs();
    }

    private void testCloseTab() {
        info("- load URL -");
        final String MEDIA_URL = getAbsoluteUrl(mStringHelper.ROBOCOP_MEDIA_PLAYBACK_LOOP_URL);
        loadUrlAndWait(MEDIA_URL);

        info("- check whether audio starts playing -");
        checkIfMediaPlayingSuccess(true /* playing */);

        info("- close audible tab -");
        final Tab tab = Tabs.getInstance().getFirstTabForUrl(MEDIA_URL);
        Tabs.getInstance().closeTab(tab);

        info("- media control should disappear -");
        waitUntilNotificationUIChanged();
        checkIfMediaNotificationBeCleared();

        info("- close tab -");
        closeAllTabs();
    }
}
