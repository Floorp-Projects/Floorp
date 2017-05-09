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

    private static final int UI_CHANGED_WAIT_MS = 6000;

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
                boolean findCorrectNotification = false;
                for (int idx = 0; idx < sbns.length; idx++) {
                    if (sbns[idx].getId() != R.id.mediaControlNotification) {
                       continue;
                    }
                    findCorrectNotification = true;
                    if (sbns[idx].getNotification().actions.length == 1 &&
                        sbns[idx].getNotification().actions[0].icon != mPrevIcon) {
                        mPrevIcon = sbns[idx].getNotification().actions[0].icon;
                        return true;
                    }
                }

                // The notification was cleared.
                if (!findCorrectNotification && mPrevIcon != 0) {
                    mPrevIcon = 0;
                    return true;
                }
                return false;
            }
        }, UI_CHANGED_WAIT_MS);
    }

    /**
     * Use these methods to wait the tab playing related states changed.
     */
    protected final void waitUntilTabMediaStarted(final Tab tab) {
        if (tab.isMediaPlaying()) {
            return;
        }
        // Tab:MediaPlaybackChange would be dispatched when media started or
        // ended, but it won't be dispatched when we pause/resume media via
        // media control.
        Actions.EventExpecter contentEventExpecter =
                mActions.expectGlobalEvent(Actions.EventType.UI, "Tab:MediaPlaybackChange");
        contentEventExpecter.blockForEvent();
        contentEventExpecter.unregisterListener();
    }

    private final void waitUntilTabAudioPlayingStateChanged(final Tab tab,
                                                            final boolean isTabPlaying) {
        if (tab.isAudioPlaying() == isTabPlaying) {
            return;
        }
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return tab.isAudioPlaying() == isTabPlaying;
            }
        }, MAX_WAIT_MS);
    }

    private final void waitUntilTabMediaPlaybackChanged(final Tab tab,
                                                        final boolean isTabPlaying) {
        if (tab.isMediaPlaying() == isTabPlaying) {
            return;
        }
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return tab.isMediaPlaying() == isTabPlaying;
            }
        }, MAX_WAIT_MS);
    }

    /**
     * These methods are used to check Tab's playing related attributes.
     * isMediaPlaying : is any media playing (might be audible or non-audbile)
     * isAudioPlaying : is any audible media playing
     */
    protected final void checkTabMediaPlayingState(final Tab tab,
                                                   final boolean isTabPlaying) {
        waitUntilTabMediaPlaybackChanged(tab, isTabPlaying);
        mAsserter.ok(isTabPlaying == tab.isMediaPlaying(),
                     "Checking the media playing state of tab, isTabPlaying = " + isTabPlaying,
                     "Tab's media playing state is correct.");
    }

    protected final void checkTabAudioPlayingState(final Tab tab,
                                                   final boolean isTabPlaying) {
        waitUntilTabAudioPlayingStateChanged(tab, isTabPlaying);
        mAsserter.ok(isTabPlaying == tab.isAudioPlaying(),
                     "Checking the audio playing state of tab, isTabPlaying = " + isTabPlaying,
                     "Tab's audio playing state is correct.");
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
     * Checking selected tab is default option.
     */
    protected final void checkIfMediaPlayingSuccess(boolean isTabPlaying) {
        checkIfMediaPlayingSuccess(isTabPlaying, false);
    }

    protected final void checkIfMediaPlayingSuccess(boolean isTabPlaying,
                                                    boolean clearNotification) {
        final Tab tab = Tabs.getInstance().getSelectedTab();
        checkTabMediaPlayingState(tab, isTabPlaying);
        checkMediaNotificationStatesAfterChanged(tab, isTabPlaying, clearNotification);

        checkTabAudioPlayingState(tab, isTabPlaying);
        checkAudioFocusStateAfterChanged(isTabPlaying);
    }

    /**
     * This method is used to check whether notification states are correct or
     * not after notification UI changed.
     */
    protected final void checkMediaNotificationStatesAfterChanged(final Tab tab,
                                                                  final boolean isTabPlaying,
                                                                  final boolean clearNotification) {
        waitUntilNotificationUIChanged();

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
        boolean findCorrectNotification = false;
        for (int idx = 0; idx < sbns.length; idx++) {
            if (sbns[idx].getId() == R.id.mediaControlNotification) {
                findCorrectNotification = true;
                break;
            }
        }
        mAsserter.ok(findCorrectNotification,
                     "Showing correct notification in system's status bar.",
                     "Check system notification");

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

        boolean findCorrectNotification = false;
        for (int idx = 0; idx < sbns.length; idx++) {
            if (sbns[idx].getId() == R.id.mediaControlNotification) {
                findCorrectNotification = true;
                break;
            }
        }
        mAsserter.ok(!findCorrectNotification,
                     "Should not have notification in system's status bar.",
                     "Check system notification.");
    }

    /**
     * This method is used to check whether audio focus state are correct or
     * not after tab's audio playing state changed.
     */
    protected final void checkAudioFocusStateAfterChanged(boolean isTabPlaying) {
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
