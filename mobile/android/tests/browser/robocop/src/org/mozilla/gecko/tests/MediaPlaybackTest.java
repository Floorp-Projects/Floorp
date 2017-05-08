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

import android.app.Notification;
import android.app.NotificationManager;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import com.robotium.solo.Condition;

abstract class MediaPlaybackTest extends BaseTest {
    private Context mContext;
    private int mPrevIcon = 0;
    private boolean mPrevTabAudioPlaying = false;

    protected final void info(String msg) {
        mAsserter.dumpLog(msg);
    }

    protected final Context getContext() {
        if (mContext == null) {
            mContext = getInstrumentation().getTargetContext();
        }
        return mContext;
    }

    /**
     * Get the system active notification and check whether its UI icon has
     * been changed.
     */
    protected final void waitUntilNotificationUIChanged() {
        if (!isAvailableToCheckNotification()) {
            return;
        }
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                NotificationManager notificationManager = (NotificationManager)
                    getContext().getSystemService(Context.NOTIFICATION_SERVICE);
                StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
                // Ensure the UI has been changed.
                if (sbns.length == 1 &&
                    sbns[0].getNotification().actions.length == 1) {
                    if (sbns[0].getNotification().actions[0].icon != mPrevIcon) {
                        mPrevIcon = sbns[0].getNotification().actions[0].icon ;
                        return true;
                    }
                }
                // The notification was cleared.
                else if (mPrevIcon != 0 && sbns.length == 0) {
                    mPrevIcon = 0;
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    /**
     * Get the selected tab and check whether tab's audio playing state has
     * been changed.
     */
    protected final void waitUntilTabAudioPlayingStateChanged() {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                Tab tab = Tabs.getInstance().getSelectedTab();
                if (tab.isAudioPlaying() != mPrevTabAudioPlaying) {
                    mPrevTabAudioPlaying = tab.isAudioPlaying();
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    /**
     * Since we can't testing media control via clicking the media control, we
     * directly send intent to service to simulate the behavior.
     */
    protected final void notifyMediaControlService(String action) {
        Intent intent = new Intent(getContext(), MediaControlService.class);
        intent.setAction(action);
        getContext().startService(intent);
    }

    /**
     * Use these methods when both media control and audio focus state should
     * be changed and you want to check whether the changing are correct or not.
     */
    protected final void checkIfMediaPlayingSuccess(boolean isTabPlaying) {
        checkIfMediaPlayingSuccess(isTabPlaying, false);
    }

    protected final void checkIfMediaPlayingSuccess(boolean isTabPlaying,
                                                    boolean clearNotification) {
        checkAudioFocusStateAfterChanged(isTabPlaying);
        checkMediaNotificationStatesAfterChanged(isTabPlaying, clearNotification);
    }

    /**
     * This method is used to check whether notification states are correct or
     * not after notification UI changed.
     */
    protected final void checkMediaNotificationStatesAfterChanged(boolean isTabPlaying,
                                                                  boolean clearNotification) {
        waitUntilNotificationUIChanged();

        final Tab tab = Tabs.getInstance().getSelectedTab();
        mAsserter.ok(isTabPlaying == tab.isMediaPlaying(),
                     "Checking the media playing state of tab, isTabPlaying = " + isTabPlaying,
                     "Tab's media playing state is correct.");

        if (clearNotification) {
            checkIfMediaNotificationBeCleared();
        } else {
            checkMediaNotificationStates(tab, isTabPlaying);
        }
    }

    protected final void checkMediaNotificationStates(final Tab tab,
                                                      final boolean isTabPlaying) {
        if (!isAvailableToCheckNotification()) {
            return;
        }
        NotificationManager notificationManager = (NotificationManager)
            getContext().getSystemService(Context.NOTIFICATION_SERVICE);

        StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
        mAsserter.is(sbns.length, 1,
                     "Should only have one notification in system's status bar.");

        Notification notification = sbns[0].getNotification();
        mAsserter.is(notification.icon,
                     R.drawable.flat_icon,
                     "Notification shows correct small icon.");
        mAsserter.is(notification.extras.get(Notification.EXTRA_TITLE),
                     tab.getTitle(),
                     "Notification shows correct title.");
        mAsserter.is(notification.extras.get(Notification.EXTRA_TEXT),
                     tab.getURL(),
                     "Notification shows correct text.");
        mAsserter.is(notification.actions.length, 1,
                     "Only has one action in notification.");
        mAsserter.is(notification.actions[0].title,
                     getContext().getString(isTabPlaying ? R.string.media_pause : R.string.media_play),
                     "Action has correct title.");
        mAsserter.is(notification.actions[0].icon,
                     isTabPlaying ? R.drawable.ic_media_pause : R.drawable.ic_media_play,
                     "Action has correct icon.");
    }

    protected final void checkIfMediaNotificationBeCleared() {
        if (!isAvailableToCheckNotification()) {
            return;
        }
        NotificationManager notificationManager = (NotificationManager)
            getContext().getSystemService(Context.NOTIFICATION_SERVICE);
        StatusBarNotification[] sbns = notificationManager.getActiveNotifications();
        mAsserter.is(sbns.length, 0,
                     "Should not have notification in system's status bar.");
    }

    /**
     * This method is used to check whether audio focus state are correct or
     * not after tab's audio playing state changed.
     */
    protected final void checkAudioFocusStateAfterChanged(boolean isTabPlaying) {
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

    protected final AudioFocusAgent getAudioFocusAgent() {
        return AudioFocusAgent.getInstance();
    }

    protected final void requestAudioFocus() {
        getAudioFocusAgent().notifyStartedPlaying();
        if (getAudioFocusAgent().getAudioFocusState() == State.OWN_FOCUS) {
            return;
        }

        // Request audio focus might fail, depend on the andriod's audio mode.
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                getAudioFocusAgent().notifyStartedPlaying();
                return getAudioFocusAgent().getAudioFocusState() == State.OWN_FOCUS;
            }
        }, MAX_WAIT_MS);
    }

    /**
     * The method NotificationManager.getActiveNotifications() is only avaiable
     * after version 23, so we need to check version ensure running the test on
     * the correct version.
     */
    protected final void checkAndroidVersionForMediaControlTest() {
        mAsserter.ok(isAvailableToCheckNotification(),
                     "Checking the android version for media control testing",
                     "The API to check system notification is only available after version 23.");
    }

    protected final boolean isAvailableToCheckNotification() {
        return Build.VERSION.SDK_INT >= 23;
    }
}
