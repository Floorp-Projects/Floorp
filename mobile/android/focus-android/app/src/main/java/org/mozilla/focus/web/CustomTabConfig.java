package org.mozilla.focus.web;

import android.content.Intent;
import android.graphics.Bitmap;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;

import org.mozilla.focus.utils.SafeIntent;

public class CustomTabConfig {
    public final @Nullable @ColorInt Integer toolbarColor;
    public final @Nullable Bitmap closeButtonIcon;

    /* package-private */ CustomTabConfig(
            final @Nullable @ColorInt Integer toolbarColor,
            final @Nullable Bitmap closeButtonIcon) {
        this.toolbarColor = toolbarColor;
        this.closeButtonIcon = closeButtonIcon;
    }

    /* package-private */ static boolean isCustomTabIntent(final @NonNull Intent intent) {
        return intent.hasExtra(CustomTabsIntent.EXTRA_SESSION);
    }

    /* package-private */ static CustomTabConfig parseCustomTabIntent(final @NonNull SafeIntent intent) {
        @ColorInt Integer toolbarColor = null;
        Bitmap closeButtonIcon = null;

        if (intent.hasExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR)) {
            toolbarColor = intent.getIntExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, -1);
        }

        if (intent.hasExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON)) {
            closeButtonIcon = intent.getParcelableExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        }

        return new CustomTabConfig(toolbarColor, closeButtonIcon);
    }
}
