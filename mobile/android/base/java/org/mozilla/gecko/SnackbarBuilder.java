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
import android.support.annotation.StringRes;
import android.support.design.widget.Snackbar;
import android.support.v4.content.ContextCompat;
import android.support.v4.widget.TextViewCompat;
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
public class SnackbarBuilder {
    /**
     * Combined interface for handling all callbacks from a snackbar because anonymous classes can only extend one
     * interface or class.
     */
    public static abstract class SnackbarCallback extends Snackbar.Callback implements View.OnClickListener {}
    public static final String LOGTAG = "GeckoSnackbarBuilder";

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

    private final Activity activity;
    private String message;
    private int duration;
    private String action;
    private SnackbarCallback callback;
    private Drawable icon;
    private Integer backgroundColor;
    private Integer actionColor;

    /**
     * @param activity Activity to show the snackbar in.
     */
    private SnackbarBuilder(final Activity activity) {
        this.activity = activity;
    }

    public static SnackbarBuilder builder(final Activity activity) {
        return new SnackbarBuilder(activity);
    }

    /**
     * @param message The text to show. Can be formatted text.
     */
    public SnackbarBuilder message(final String message) {
        this.message = message;
        return this;
    }

    /**
     * @param id The id of the string resource to show. Can be formatted text.
     */
    public SnackbarBuilder message(@StringRes final int id) {
        message = activity.getResources().getString(id);
        return this;
    }

    /**
     * @param duration How long to display the message.
     */
    public SnackbarBuilder duration(final int duration) {
        this.duration = duration;
        return this;
    }

    /**
     * @param action Action text to display.
     */
    public SnackbarBuilder action(final String action) {
        this.action = action;
        return this;
    }

    /**
     * @param id The id of the string resource for the action text to display.
     */
    public SnackbarBuilder action(@StringRes final int id) {
        action = activity.getResources().getString(id);
        return this;
    }

    /**
     * @param callback Callback to be invoked when the action is clicked or the snackbar is dismissed.
     */
    public SnackbarBuilder callback(final SnackbarCallback callback) {
        this.callback = callback;
        return this;
    }

    /**
     * @param callback Callback to be invoked when the action is clicked or the snackbar is dismissed.
     */
    public SnackbarBuilder callback(final EventCallback callback) {
        this.callback = new SnackbarEventCallback(callback);
        return this;
    }

    /**
     * @param icon Icon to be displayed with the snackbar text.
     */
    public SnackbarBuilder icon(final Drawable icon) {
        this.icon = icon;
        return this;
    }

    /**
     * @param backgroundColor Snackbar background color.
     */
    public SnackbarBuilder backgroundColor(final Integer backgroundColor) {
        this.backgroundColor = backgroundColor;
        return this;
    }

    /**
     * @param actionColor Action text color.
     */
    public SnackbarBuilder actionColor(final Integer actionColor) {
        this.actionColor = actionColor;
        return this;
    }

    /**
     * @param object Populate the builder with data from a Gecko Snackbar:Show event.
     */
    public SnackbarBuilder fromEvent(final NativeJSObject object) {
        message = object.getString("message");
        duration = object.getInt("duration");

        if (object.has("backgroundColor")) {
            final String providedColor = object.getString("backgroundColor");
            try {
                backgroundColor = Color.parseColor(providedColor);
            } catch (IllegalArgumentException e) {
                Log.w(LOGTAG, "Failed to parse color string: " + providedColor);
            }
        }

        NativeJSObject actionObject = object.optObject("action", null);
        if (actionObject != null) {
            action = actionObject.optString("label", null);
        }
        return this;
    }

    public void buildAndShow() {
        final View parentView = findBestParentView(activity);
        final Snackbar snackbar = Snackbar.make(parentView, message, duration);

        if (callback != null && !TextUtils.isEmpty(action)) {
            snackbar.setAction(action, callback);
            if (actionColor == null) {
                snackbar.setActionTextColor(ContextCompat.getColor(activity, R.color.fennec_ui_orange));
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
            TextViewCompat.setCompoundDrawablesRelative(textView, paddedIcon, null, null, null);
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
