/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.customtabs;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Parcelable;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.customtabs.CustomTabsIntent;
import android.util.Log;

import org.mozilla.focus.R;
import org.mozilla.focus.utils.SafeBundle;
import org.mozilla.focus.utils.SafeIntent;

import java.util.ArrayList;
import java.util.Collections;
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

    private final @NonNull List<String> unsupportedFeatureList;

    /* package-private */ CustomTabConfig(
            final @Nullable @ColorInt Integer toolbarColor,
            final @Nullable Bitmap closeButtonIcon,
            final boolean disableUrlbarHiding,
            final @Nullable ActionButtonConfig actionButtonConfig,
            final boolean showShareMenuItem,
            final @NonNull List<CustomTabMenuItem> menuItems,
            final @NonNull List<String> unsupportedFeatureList) {
        this.toolbarColor = toolbarColor;
        this.closeButtonIcon = closeButtonIcon;
        this.disableUrlbarHiding = disableUrlbarHiding;
        this.actionButtonConfig = actionButtonConfig;
        this.showShareMenuItem = showShareMenuItem;
        this.menuItems = menuItems;
        this.unsupportedFeatureList = unsupportedFeatureList;
    }

    public static boolean isCustomTabIntent(final @NonNull SafeIntent intent) {
        return intent.hasExtra(CustomTabsIntent.EXTRA_SESSION);
    }

    /* package-private */ static @Nullable Bitmap getCloseButtonIcon(final @NonNull Context context, final @NonNull SafeIntent intent) {
        if (!intent.hasExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON)) {
            return null;
        }

        final Parcelable closeButtonParcelable = intent.getParcelableExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON);
        if (!(closeButtonParcelable instanceof Bitmap)) {
            return null;
        }

        final Bitmap candidateIcon = (Bitmap) closeButtonParcelable;
        final int maxSize = context.getResources().getDimensionPixelSize(R.dimen.customtabs_toolbar_icon_size);

        if (candidateIcon.getWidth() <= maxSize &&
                candidateIcon.getHeight() <= maxSize) {
            return candidateIcon;
        } else {
            candidateIcon.recycle();
            return null;
        }
    }

    /* package-private */ static @Nullable ActionButtonConfig getActionButtonConfig(final @NonNull Context context, final @NonNull SafeIntent intent) {
        if (!intent.hasExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE)) {
            return null;
        }

        final SafeBundle actionButtonBundle = intent.getBundleExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE);

        final String description = actionButtonBundle.getString(CustomTabsIntent.KEY_DESCRIPTION);
        if (description == null) {
            Log.w(LOGTAG, "Ignoring EXTRA_ACTION_BUTTON_BUNDLE due to missing description");
            return null;
        }

        final Parcelable pendingIntentParcelable = actionButtonBundle.getParcelable(CustomTabsIntent.KEY_PENDING_INTENT);
        final PendingIntent pendingIntent;
        // See below: this might not be a PendingIntent, we need to verify ourselves
        if (pendingIntentParcelable instanceof  PendingIntent) {
            pendingIntent = (PendingIntent) pendingIntentParcelable;
        } else {
            Log.w(LOGTAG, "Ignoring EXTRA_ACTION_BUTTON_BUNDLE due to missing pendingIntent");
            return null;
        }

        // Process the icon last: if we don't use the icon we need to recycle it, but if we've gotten
        // to this stage we know that we have a valid description and pendingIntent, so returning
        // an ActionButtonConfig is definitely possible (unless there's no icon, in which case there's nothing to do):
        final Parcelable iconParcelable = actionButtonBundle.getParcelable(CustomTabsIntent.KEY_ICON);
        final Bitmap icon;
        // Java Generics mean that this parcelable might not be a bitmap, so we need to check and
        // safely cast ourselves:
        if (iconParcelable instanceof Bitmap) {
            icon = (Bitmap) iconParcelable;
        } else {
            Log.w(LOGTAG, "Ignoring EXTRA_ACTION_BUTTON_BUNDLE due to missing icon");
            return null;
        }

        return new ActionButtonConfig(description, icon, pendingIntent);
    }

    public static CustomTabConfig parseCustomTabIntent(final @NonNull Context context, final @NonNull SafeIntent intent) {
        @ColorInt Integer toolbarColor = null;
        if (intent.hasExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR)) {
            toolbarColor = intent.getIntExtra(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, -1);
        }

        final Bitmap closeButtonIcon = getCloseButtonIcon(context, intent);

        // Custom tabs in Chrome defaults to hiding the URL bar. CustomTabsIntent.Builder only offers
        // enableUrlBarHiding() which sets this value to true, which would suggest that the default
        // is to have this disabled - but that's not what chrome does, I'm currently waiting
        // for feedback on this bug: https://bugs.chromium.org/p/chromium/issues/detail?id=718654
        boolean disableUrlbarHiding = !intent.getBooleanExtra(CustomTabsIntent.EXTRA_ENABLE_URLBAR_HIDING, true);

        final ActionButtonConfig actionButtonConfig = getActionButtonConfig(context, intent);

        // Share is part of the default menu, so it's simplest just to toggle it off as necessary instead
        // of creating a fake menu item here, hence we keep this as  aboolean for now:
        final boolean showShareMenuItem = intent.getBooleanExtra(CustomTabsIntent.EXTRA_DEFAULT_SHARE_MENU_ITEM,
                // Other than Chrome we will show the share menu by default. Note that currently the
                // Custom Tabs support library has no option to explicitly hide this menu item. So
                // setting the default to 'true' will show this menu item for all custom tabs.
                true);

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

        // Store a list of unsupported features: we'll need these for telemetry. Eventually we'll want
        // to actually parse these items, and use them in frontend code.
        final List<String> unsupportedFeatureList = new LinkedList<>();

        if (intent.hasExtra(CustomTabsIntent.EXTRA_TINT_ACTION_BUTTON)) {
            unsupportedFeatureList.add("hasActionButtonTint");
        }

        // The docs mention that EXTRA_TOOLBAR_ITEMS can be on the top or bottom toolbars. In practice
        // (in Chrome) they are inserted into the bottom toolbar:
        if (intent.hasExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS) ||
                intent.hasExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS)) {
            unsupportedFeatureList.add("hasBottomToolbar");
        }

        if (intent.hasExtra(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR)) {
            // Disabled to not send more than 10 telemetry extras (See #629): hasBottomToolbar will
            // be enough to determine whether this is a feature we should support.

            // unsupportedFeatureList.add("hasBottomToolbarColor");
        }

        if (intent.hasExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE)) {
            unsupportedFeatureList.add("hasExitAnimation");
        }

        if (intent.hasExtra(CustomTabsIntent.EXTRA_ENABLE_INSTANT_APPS)) {
            // Disabled to not send more than 10 telemetry extras (See #629): Instant apps are not
            // something we are interested in here.

            // unsupportedFeatureList.add("enablesInstantApps");
        }

        if (intent.hasExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE)) {
            final int titleVisibility = intent.getIntExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, 0);
            switch (titleVisibility) {
                case CustomTabsIntent.SHOW_PAGE_TITLE:
                    unsupportedFeatureList.add("hasPageTitle");
                    break;
                case CustomTabsIntent.NO_TITLE:
                    break;
                default:
                    Log.w(LOGTAG, "Custom tab intent specified unknown EXTRA_TITLE_VISIBILITY_STATE");
                    break;
            }
        }

        return new CustomTabConfig(toolbarColor, closeButtonIcon, disableUrlbarHiding, actionButtonConfig, showShareMenuItem, menuItems, unsupportedFeatureList);
    }

    /**
     * Get a list of options enabled in the custom tabs intent, e.g. [hasToolbarColor, hasCloseButton].
     */
    public List<String> getOptionsList() {
        // A list of custom-tab features that are used, stored for telemetry purposed
        final List<String> featureList = new LinkedList<>(unsupportedFeatureList);

        if (toolbarColor != null) {
            featureList.add("hasToolbarColor");
        }

        if (closeButtonIcon != null) {
            featureList.add("hasCloseButton");
        }

        if (!disableUrlbarHiding) {
            featureList.add("disablesUrlbarHiding");
        }

        if (actionButtonConfig != null) {
            featureList.add("hasActionButton");
        }

        if (showShareMenuItem) {
            featureList.add("hasShareItem");
        }

        if (menuItems.size() > 0) {
            featureList.add("hasCustomizedMenu");
        }

        return Collections.unmodifiableList(featureList);
    }
}
