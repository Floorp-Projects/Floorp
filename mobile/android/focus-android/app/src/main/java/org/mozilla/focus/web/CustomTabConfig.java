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
    public final boolean disableUrlbarHiding;


    /* package-private */ CustomTabConfig(
            final @Nullable @ColorInt Integer toolbarColor,
            final @Nullable Bitmap closeButtonIcon,
            final boolean disableUrlbarHiding) {
        this.toolbarColor = toolbarColor;
        this.closeButtonIcon = closeButtonIcon;
        this.disableUrlbarHiding = disableUrlbarHiding;
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

        return new CustomTabConfig(toolbarColor, closeButtonIcon, disableUrlbarHiding);
    }
}
