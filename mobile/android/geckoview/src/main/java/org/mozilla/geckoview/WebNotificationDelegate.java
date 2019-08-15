package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;

import org.mozilla.gecko.annotation.WrapForJNI;

public interface WebNotificationDelegate {
    /**
     * This is called when a new notification is created.
     *
     * @param notification The WebNotification received.
     */
    @AnyThread
    @WrapForJNI
    default void onShowNotification(@NonNull WebNotification notification) {}

    /**
     * This is called when an existing notification is closed.
     *
     * @param notification The WebNotification received.
     */
    @AnyThread
    @WrapForJNI
    default void onCloseNotification(@NonNull WebNotification notification) {}
}

