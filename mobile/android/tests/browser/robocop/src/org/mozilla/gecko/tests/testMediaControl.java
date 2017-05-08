/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.media.AudioFocusAgent;
import org.mozilla.gecko.media.AudioFocusAgent.State;
import org.mozilla.gecko.media.MediaControlService;

import android.content.Intent;
import android.content.Context;
import android.media.AudioManager;

import android.app.Notification;
import android.app.NotificationManager;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import com.robotium.solo.Condition;

public class testMediaControl extends BaseTest {
    private Context mContext;
    private int mPrevIcon = 0;
    private boolean mPrevTabAudioPlaying = false;

    public void testMediaControl() {
        // The API to check system notification is available after version 23.
        if (Build.VERSION.SDK_INT < 23) {
            return;
        }

        info("- wait for gecko ready -");
        blockForGeckoReady();

        info("- setup testing memeber variable -");
        setupForTesting();

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

        info("- run next test : testNavigateOutThePage -");
        testNavigateOutThePage();
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

        info("- run next test : testSwitchTab -");
        testSwitchTab();
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

        info("- the media control should still be displayed in status bar -");
        checkDisplayedNotificationStates(true /* playing */);

        info("- close tab -");
        closeAllTabs();
    }

    /**
     * Testing tool functions
     */
    private void info(String msg) {
        mAsserter.dumpLog(msg);
    }

    private void setupForTesting() {
        mContext = getInstrumentation().getTargetContext();
    }

    private void waitUntilNotificationUIChanged() {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                NotificationManager notificationManager = (NotificationManager)
                    mContext.getSystemService(Context.NOTIFICATION_SERVICE);
                StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
                if (sbns.length == 1 &&
                    sbns[0].getNotification().actions.length == 1) {
                    // Ensure the UI has been changed.
                    if (sbns[0].getNotification().actions[0].icon != mPrevIcon) {
                        mPrevIcon = sbns[0].getNotification().actions[0].icon ;
                        return true;
                    }
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    private void waitUntilTabAudioPlayingStateChanged() {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                if (tab.isAudioPlaying() != mPrevTabAudioPlaying) {
                    mPrevTabAudioPlaying = tab.isAudioPlaying();
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    private void notifyMediaControlService(String action) {
        Intent intent = new Intent(mContext, MediaControlService.class);
        intent.setAction(action);
        mContext.startService(intent);
    }

    private void checkIfMediaPlayingSuccess(boolean isTabPlaying) {
        checkIfMediaPlayingSuccess(isTabPlaying, false);
    }

    private void checkIfMediaPlayingSuccess(boolean isTabPlaying,
                                            boolean clearNotification) {
        checkAudioFocusStateChanged(isTabPlaying);
        checkMediaNotificationStatesChanged(isTabPlaying, clearNotification);
    }

    /**
     * This method is used to check whether notification states are correct or
     * not after notification UI changed.
     */
    private void checkMediaNotificationStatesChanged(boolean isTabPlaying,
                                                     boolean clearNotification) {
        waitUntilNotificationUIChanged();

        final Tab tab = Tabs.getInstance().getSelectedTab();
        mAsserter.ok(isTabPlaying == tab.isMediaPlaying(),
                     "Checking the media playing state of tab, isTabPlaying = " + isTabPlaying,
                     "Tab's media playing state is correct.");

        if (clearNotification) {
            checkIfNotificationBeCleared();
        } else {
            checkDisplayedNotificationStates(isTabPlaying);
        }
    }

    private void checkDisplayedNotificationStates(boolean isTabPlaying) {
        NotificationManager notificationManager = (NotificationManager)
            mContext.getSystemService(Context.NOTIFICATION_SERVICE);

        StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
        mAsserter.is(sbns.length, 1,
                     "Should only have one notification in system's status bar.");

        Notification notification = sbns[0].getNotification();
        mAsserter.is(notification.actions.length, 1,
                     "Only has one action in notification.");

        mAsserter.is(notification.actions[0].title,
                     mContext.getString(isTabPlaying ? R.string.media_pause : R.string.media_play),
                     "Action has correct title.");
        mAsserter.is(notification.actions[0].icon,
                     isTabPlaying ? R.drawable.ic_media_pause : R.drawable.ic_media_play,
                     "Action has correct icon.");
    }

    private void checkIfNotificationBeCleared() {
        NotificationManager notificationManager = (NotificationManager)
            mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
        mAsserter.is(sbns.length, 0,
                     "Should not have notification in system's status bar.");
    }

    /**
     * This method is used to check whether audio focus state are correct or
     * not after tab's audio playing state changed.
     */
    private void checkAudioFocusStateChanged(boolean isTabPlaying) {
        waitUntilTabAudioPlayingStateChanged();

        final Tab tab = Tabs.getInstance().getSelectedTab();
        mAsserter.ok(isTabPlaying == tab.isAudioPlaying(),
                     "Checking the audio playing state of tab, isTabPlaying = " + isTabPlaying,
                     "Tab's audio playing state is correct.");

        if (isTabPlaying) {
            mAsserter.is(AudioFocusAgent.getInstance().getAudioFocusState(),
                         State.OWN_FOCUS,
                         "Audio focus state is correct.");
        } else {
            boolean isLostFocus =
                AudioFocusAgent.getInstance().getAudioFocusState().equals(State.LOST_FOCUS) ||
                AudioFocusAgent.getInstance().getAudioFocusState().equals(State.LOST_FOCUS_TRANSIENT);
            mAsserter.ok(isLostFocus,
                         "Checking the audio focus when the tab is not playing",
                         "Audio focus state is correct.");
        }
    }

    private AudioFocusAgent getAudioFocusAgent() {
        return AudioFocusAgent.getInstance();
    }
}
