/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeJSObject;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;
import android.util.Log;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import java.lang.ref.WeakReference;

/**
 * Helper class for creating and dismissing snackbars. Use this class to guarantee a consistent style and behavior
 * across the app.
 */
public class SnackbarHelper {
    /**
     * Combined interface for handling all callbacks from a snackbar because anonymous classes can only extend one
     * interface or class.
     */
    public static abstract class SnackbarCallback extends Snackbar.Callback implements View.OnClickListener {}
    public static final String LOGTAG = "GeckoSnackbarHelper";

    /**
     * SnackbarCallback implementation for delegating snackbar events to an EventCallback.
     */
    private static class SnackbarEventCallback extends SnackbarCallback {
        private EventCallback callback;

        public SnackbarEventCallback(EventCallback callback) {
            this.callback = callback;
        }

        @Override
        public synchronized void onClick(View view) {
            if (callback == null) {
                return;
            }

            callback.sendSuccess(null);
            callback = null; // Releasing reference. We only want to execute the callback once.
        }

        @Override
        public synchronized void onDismissed(Snackbar snackbar, int event) {
            if (callback == null || event == Snackbar.Callback.DISMISS_EVENT_ACTION) {
                return;
            }

            callback.sendError(null);
            callback = null; // Releasing reference. We only want to execute the callback once.
        }
    }

    private static final Object currentSnackbarLock = new Object();
    private static WeakReference<Snackbar> currentSnackbar = new WeakReference<>(null); // Guarded by 'currentSnackbarLock'

    /**
     * Show a snackbar to display a message.
     *
     * @param activity Activity to show the snackbar in.
     * @param message The text to show. Can be formatted text.
     * @param duration How long to display the message.
     */
    public static void showSnackbar(Activity activity, String message, int duration) {
        showSnackbarWithAction(activity, message, duration, null, null);
    }

    /**
     * Build and show a snackbar from a Gecko Snackbar:Show event.
     */
    public static void showSnackbar(Activity activity, final NativeJSObject object, final EventCallback callback) {
        final String message = object.getString("message");
        final int duration = object.getInt("duration");

        Integer backgroundColor = null;

        if (object.has("backgroundColor")) {
            final String providedColor = object.getString("backgroundColor");
            try {
                backgroundColor = Color.parseColor(providedColor);
            } catch (IllegalArgumentException e) {
                Log.w(LOGTAG, "Failed to parse color string: " + providedColor);
            }
        }

        NativeJSObject action = object.optObject("action", null);

        showSnackbarWithActionAndColors(activity,
                message,
                duration,
                action != null ? action.optString("label", null) : null,
                new SnackbarHelper.SnackbarEventCallback(callback),
                null,
                backgroundColor,
                null
        );
    }

    /**
     * Show a snackbar to display a message and an action.
     *
     * @param activity Activity to show the snackbar in.
     * @param message The text to show. Can be formatted text.
     * @param duration How long to display the message.
     * @param action Action text to display.
     * @param callback Callback to be invoked when the action is clicked or the snackbar is dismissed.
     */
    public static void showSnackbarWithAction(Activity activity, String message, int duration, String action, SnackbarCallback callback) {
        showSnackbarWithActionAndColors(activity, message, duration, action, callback, null, null, null);
    }


    public static void showSnackbarWithActionAndColors(Activity activity,
                                                       String message,
                                                       int duration,
                                                       String action,
                                                       SnackbarCallback callback,
                                                       Drawable icon,
                                                       Integer backgroundColor,
                                                       Integer actionColor) {
        final View parentView = findBestParentView(activity);
        final Snackbar snackbar = Snackbar.make(parentView, message, duration);

        if (callback != null && !TextUtils.isEmpty(action)) {
            snackbar.setAction(action, callback);
            if (actionColor == null) {
                ContextCompat.getColor(activity, R.color.fennec_ui_orange);
            } else {
                snackbar.setActionTextColor(actionColor);
            }
            snackbar.setCallback(callback);
        }

        if (icon != null) {
            int leftPadding = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 10, activity.getResources().getDisplayMetrics());

            final InsetDrawable paddedIcon = new InsetDrawable(icon, 0, 0, leftPadding, 0);

            paddedIcon.setBounds(0, 0, leftPadding + icon.getIntrinsicWidth(), icon.getIntrinsicHeight());

            TextView textView = (TextView) snackbar.getView().findViewById(android.support.design.R.id.snackbar_text);
            textView.setCompoundDrawables(paddedIcon, null, null, null);
        }

        if (backgroundColor != null) {
            snackbar.getView().setBackgroundColor(backgroundColor);
        }

        snackbar.show();

        synchronized (currentSnackbarLock) {
            currentSnackbar = new WeakReference<>(snackbar);
        }
    }

    /**
     * Dismiss the currently visible snackbar.
     */
    public static void dismissCurrentSnackbar() {
        synchronized (currentSnackbarLock) {
            final Snackbar snackbar = currentSnackbar.get();
            if (snackbar != null && snackbar.isShown()) {
                snackbar.dismiss();
            }
        }
    }

    /**
     * Find the best parent view to hold the Snackbar's view. The Snackbar implementation of the support
     * library will use this view to walk up the view tree to find an actual suitable parent (if needed).
     */
    private static View findBestParentView(Activity activity) {
        if (activity instanceof GeckoApp) {
            final View view = activity.findViewById(R.id.root_layout);
            if (view != null) {
                return view;
            }
        }

        return activity.findViewById(android.R.id.content);
    }
}
