package org.mozilla.focus.web;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.util.Log;

import org.mozilla.focus.utils.SafeIntent;

public class CustomTabConfig {
    private static final String LOGTAG = "CustomTabConfig";

    public static final class ActionButtonConfig {
        public final @NonNull String description;
        public final @NonNull Bitmap icon;
        public final @NonNull PendingIntent pendingIntent;

        public ActionButtonConfig(final @NonNull String description,
                                  final @NonNull Bitmap icon,
                                  final @NonNull PendingIntent pendingIntent) {
            this.description = description;
            this.icon = icon;
            this.pendingIntent = pendingIntent;
        }
    }

    public final @Nullable @ColorInt Integer toolbarColor;
    public final @Nullable Bitmap closeButtonIcon;
    public final boolean disableUrlbarHiding;

    public final @Nullable ActionButtonConfig actionButtonConfig;

    /* package-private */ CustomTabConfig(
            final @Nullable @ColorInt Integer toolbarColor,
            final @Nullable Bitmap closeButtonIcon,
            final boolean disableUrlbarHiding,
            final @Nullable ActionButtonConfig actionButtonConfig) {
        this.toolbarColor = toolbarColor;
        this.closeButtonIcon = closeButtonIcon;
        this.disableUrlbarHiding = disableUrlbarHiding;
        this.actionButtonConfig = actionButtonConfig;
    }

    /* package-private */ static boolean isCustomTabIntent(final @NonNull Intent intent) {
        return intent.hasExtra(CustomTabsIntent.EXTRA_SESSION);
    }

    /* package-private */ static CustomTabConfig parseCustomTabIntent(final @NonNull SafeIntent intent) {
        @ColorInt Integer toolbarColor = null;
        if (intent.hasExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR)) {
            toolbarColor = intent.getIntExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, -1);
        }

        Bitmap closeButtonIcon = null;
        if (intent.hasExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON)) {
            closeButtonIcon = intent.getParcelableExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        }

        // Custom tabs in Chrome defaults to hiding the URL bar. CustomTabsIntent.Builder only offers
        // enableUrlBarHiding() which sets this value to true, which would suggest that the default
        // is to have this disabled - but that's not what chrome does, I'm currently waiting
        // for feedback on this bug: https://bugs.chromium.org/p/chromium/issues/detail?id=718654
        boolean disableUrlbarHiding = !intent.getBooleanExtra(CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, true);

        ActionButtonConfig actionButtonConfig = null;
        if (intent.hasExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE)) {
            final Bundle actionButtonBundle = intent.getBundleExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE);

            final String description = actionButtonBundle.getString(CustomTabsIntent.KEY_DESCRIPTION);
            final Bitmap icon = actionButtonBundle.getParcelable(CustomTabsIntent.KEY_ICON);
            final PendingIntent pendingIntent = actionButtonBundle.getParcelable(CustomTabsIntent.KEY_PENDING_INTENT);

            if (description != null &&
                    icon != null &&
                    pendingIntent != null) {
                actionButtonConfig = new ActionButtonConfig(description, icon, pendingIntent);
            } else {
                Log.w(LOGTAG, "Ignoring EXTRA_ACTION_BUTTON_BUNDLE due to missing keys");
            }
        }

        return new CustomTabConfig(toolbarColor, closeButtonIcon, disableUrlbarHiding, actionButtonConfig);
    }
}
