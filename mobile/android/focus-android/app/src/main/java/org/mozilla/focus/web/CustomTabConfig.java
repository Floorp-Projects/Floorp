package org.mozilla.focus.web;

import android.app.PendingIntent;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.util.Log;

import org.mozilla.focus.utils.SafeBundle;
import org.mozilla.focus.utils.SafeIntent;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

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

    public static final class CustomTabMenuItem {
        public final String name;
        public final PendingIntent pendingIntent;

        public CustomTabMenuItem(final @NonNull String name, final @NonNull PendingIntent pendingIntent) {
            this.name = name;
            this.pendingIntent = pendingIntent;
        }
    }

    public final @Nullable @ColorInt Integer toolbarColor;
    public final @Nullable Bitmap closeButtonIcon;
    public final boolean disableUrlbarHiding;

    public final @Nullable ActionButtonConfig actionButtonConfig;
    public final boolean showShareMenuItem;
    public final @NonNull List<CustomTabMenuItem> menuItems;

    /* package-private */ CustomTabConfig(
            final @Nullable @ColorInt Integer toolbarColor,
            final @Nullable Bitmap closeButtonIcon,
            final boolean disableUrlbarHiding,
            final @Nullable ActionButtonConfig actionButtonConfig,
            final boolean showShareMenuItem,
            final @NonNull List<CustomTabMenuItem> menuItems) {
        this.toolbarColor = toolbarColor;
        this.closeButtonIcon = closeButtonIcon;
        this.disableUrlbarHiding = disableUrlbarHiding;
        this.actionButtonConfig = actionButtonConfig;
        this.showShareMenuItem = showShareMenuItem;
        this.menuItems = menuItems;
    }

    /* package-private */ static boolean isCustomTabIntent(final @NonNull SafeIntent intent) {
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

        // Share is part of the default menu, so it's simplest just to toggle it off as necessary instead
        // of creating a fake menu item here, hence we keep this as  aboolean for now:
        final boolean showShareMenuItem = intent.getBooleanExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM, false);

        final List<CustomTabMenuItem> menuItems = new LinkedList<>();
        if (intent.hasExtra(CustomTabsIntent.EXTRA_MENU_ITEMS)) {
            // This ArrayList should contain Bundles, however java generics don't actually let
            // us check that that's the case (the only guarantee we get is that it's an ArrayList):
            // You can see some gory details of similar issues at:
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1280382#c17
            final ArrayList<?> menuItemBundles = intent.getParcelableArrayListExtra(CustomTabsIntent.EXTRA_MENU_ITEMS);

            for (final Object bundleObject : menuItemBundles) {
                if (!(bundleObject instanceof Bundle)) {
                    // As noted above, this might not be a bundle:
                    continue;
                }

                final SafeBundle bundle = new SafeBundle((Bundle) bundleObject);

                final String name = bundle.getString(CustomTabsIntent.KEY_MENU_ITEM_TITLE);

                final Parcelable parcelableIntent = bundle.getParcelable(CustomTabsIntent.KEY_PENDING_INTENT);
                if (!(parcelableIntent instanceof PendingIntent)) {
                    // And again: java generics don't guarantee anything, so we need to check for
                    // PendingIntents ourselves
                    continue;
                }

                final PendingIntent pendingIntent = (PendingIntent) parcelableIntent;

                menuItems.add(new CustomTabMenuItem(name, pendingIntent));
            }
        }

        return new CustomTabConfig(toolbarColor, closeButtonIcon, disableUrlbarHiding, actionButtonConfig, showShareMenuItem, menuItems);
    }
}
