/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.support.annotation.AnyThread;
import android.support.annotation.LongDef;
import android.support.annotation.NonNull;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * Manage runtime storage data.
 *
 * Retrieve an instance via {@link GeckoRuntime#getStorageController}.
 */
public final class StorageController {

    // Keep in sync with GeckoViewStorageController.ClearFlags.
    /**
     * Flags used for data clearing operations.
     */
    public static class ClearFlags {
        /**
         * Cookies.
         */
        public static final long COOKIES = 1 << 0;

        /**
         * Network cache.
         */
        public static final long NETWORK_CACHE = 1 << 1;

        /**
         * Image cache.
         */
        public static final long IMAGE_CACHE = 1 << 2;

        /**
         * DOM storages.
         */
        public static final long DOM_STORAGES = 1 << 4;

        /**
         * Auth tokens and caches.
         */
        public static final long AUTH_SESSIONS = 1 << 5;

        /**
         * Site permissions.
         */
        public static final long PERMISSIONS = 1 << 6;

        /**
         * All caches.
         */
        public static final long ALL_CACHES = NETWORK_CACHE | IMAGE_CACHE;

        /**
         * All site settings (permissions, content preferences, security
         * settings, etc.).
         */
        public static final long SITE_SETTINGS = 1 << 7 | PERMISSIONS;

        /**
         * All site-related data (cookies, storages, caches, permissions, etc.).
         */
        public static final long SITE_DATA =
            1 << 8 | COOKIES | DOM_STORAGES | ALL_CACHES |
            PERMISSIONS | SITE_SETTINGS;

        /**
         * All data.
         */
        public static final long ALL = 1 << 9;
    }

    @Retention(RetentionPolicy.SOURCE)
    @LongDef(flag = true,
             value = { ClearFlags.COOKIES,
                       ClearFlags.NETWORK_CACHE,
                       ClearFlags.IMAGE_CACHE,
                       ClearFlags.DOM_STORAGES,
                       ClearFlags.AUTH_SESSIONS,
                       ClearFlags.PERMISSIONS,
                       ClearFlags.ALL_CACHES,
                       ClearFlags.SITE_SETTINGS,
                       ClearFlags.SITE_DATA,
                       ClearFlags.ALL })
    /* package */ @interface StorageControllerClearFlags {}

    /**
     * Clear data for all hosts.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions prior to clearing data.
     *
     * @param flags Combination of {@link ClearFlags}.
     * @return A {@link GeckoResult} that will complete when clearing has
     *         finished.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> clearData(
            final @StorageControllerClearFlags long flags) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putLong("flags", flags);

        EventDispatcher.getInstance().dispatch(
            "GeckoView:ClearData", bundle, result);

        return result;
    }

    /**
     * Clear data owned by the given host.
     * Clearing data for a host will not clear data created by its third-party
     * origins.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions prior to clearing data.
     *
     * @param host The host to be used.
     * @param flags Combination of {@link ClearFlags}.
     * @return A {@link GeckoResult} that will complete when clearing has
     *         finished.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> clearDataFromHost(
            final @NonNull String host,
            final @StorageControllerClearFlags long flags) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("host", host);
        bundle.putLong("flags", flags);

        EventDispatcher.getInstance().dispatch(
            "GeckoView:ClearHostData", bundle, result);

        return result;
    }
}
