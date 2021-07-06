/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.util.List;
import java.util.Locale;

import androidx.annotation.AnyThread;
import androidx.annotation.LongDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission;

/**
 * Manage runtime storage data.
 *
 * Retrieve an instance via {@link GeckoRuntime#getStorageController}.
 */
public final class StorageController {
    private static final String LOGTAG = "StorageController";

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
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putLong("flags", flags);

        return EventDispatcher.getInstance()
                .queryVoid("GeckoView:ClearData", bundle);
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
        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("host", host);
        bundle.putLong("flags", flags);

        return EventDispatcher.getInstance()
                .queryVoid("GeckoView:ClearHostData", bundle);
    }

    /**
     * Clear data owned by the given base domain (eTLD+1).
     * Clearing data for a base domain will also clear any associated
     * third-party storage. This includes clearing for third-parties embedded by
     * the domain and for the given domain embedded under other sites.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions prior to clearing data.
     *
     * @param baseDomain The base domain to be used.
     * @param flags Combination of {@link ClearFlags}.
     * @return A {@link GeckoResult} that will complete when clearing has
     *         finished.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> clearDataFromBaseDomain(
            final @NonNull String baseDomain,
            final @StorageControllerClearFlags long flags) {
        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("baseDomain", baseDomain);
        bundle.putLong("flags", flags);

        return EventDispatcher.getInstance()
                .queryVoid("GeckoView:ClearBaseDomainData", bundle);
    }

    /**
     * Clear data for the given context ID.
     * Use {@link GeckoSessionSettings.Builder#contextId}.to set a context ID
     * for a session.
     *
     * Note: Any open session may re-accumulate previously cleared data. To
     * ensure that no persistent data is left behind, you need to close all
     * sessions for the given context prior to clearing data.
     *
     * @param contextId The context ID for the storage data to be deleted.
     */
    @AnyThread
    public void clearDataForSessionContext(final @NonNull String contextId) {
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("contextId", createSafeSessionContextId(contextId));

        EventDispatcher.getInstance().dispatch(
            "GeckoView:ClearSessionContextData", bundle);
    }

    /* package */ static @Nullable String createSafeSessionContextId(
            final @Nullable String contextId) {
        if (contextId == null) {
            return null;
        }
        if (contextId.isEmpty()) {
            // Let's avoid empty strings for Gecko.
            return "gvctxempty";
        }
        // We don't want to restrict the session context ID string options, so to
        // ensure that the string is safe for Gecko processing, we translate it to
        // its hex representation.
        return String.format("gvctx%x", new BigInteger(contextId.getBytes()))
               .toLowerCase(Locale.ROOT);
    }

    /* package */ static @Nullable String retrieveUnsafeSessionContextId(
            final @Nullable String contextId) {
        if (contextId == null || contextId.isEmpty()) {
            return null;
        }
        if ("gvctxempty".equals(contextId)) {
            return "";
        }
        final byte[] bytes = new BigInteger(contextId.substring(5), 16).toByteArray();
        return new String(bytes, Charset.forName("UTF-8"));
    }

    /**
     * Get all currently stored permissions.
     *
     * @return A {@link GeckoResult} that will complete with a list of all
     *         currently stored {@link ContentPermission}s.
     */
    @AnyThread
    public @NonNull GeckoResult<List<ContentPermission>> getAllPermissions() {
        return EventDispatcher.getInstance().queryBundle("GeckoView:GetAllPermissions").map(bundle -> {
            final GeckoBundle[] permsArray = bundle.getBundleArray("permissions");
            return ContentPermission.fromBundleArray(permsArray);
        });
    }

    /**
     * Get all currently stored permissions for a given URI and default (unset) context ID.
     *
     * @param uri A String representing the URI to get permissions for.
     *
     * @return A {@link GeckoResult} that will complete with a list of all
     *         currently stored {@link ContentPermission}s for the URI.
     */
    @AnyThread
    public @NonNull GeckoResult<List<ContentPermission>> getPermissions(final @NonNull String uri) {
        return getPermissions(uri, null);
    }

    /**
     * Get all currently stored permissions for a given URI and context ID.
     *
     * @param uri A String representing the URI to get permissions for.
     * @param contextId A String specifying the context ID.
     *
     * @return A {@link GeckoResult} that will complete with a list of all
     *         currently stored {@link ContentPermission}s for the URI.
     */
    @AnyThread
    public @NonNull GeckoResult<List<ContentPermission>> getPermissions(final @NonNull String uri, final @Nullable String contextId) {
        final GeckoBundle msg = new GeckoBundle(2);
        msg.putString("uri", uri);
        msg.putString("contextId", createSafeSessionContextId(contextId));
        return EventDispatcher.getInstance().queryBundle("GeckoView:GetPermissionsByURI", msg).map(bundle -> {
            final GeckoBundle[] permsArray = bundle.getBundleArray("permissions");
            return ContentPermission.fromBundleArray(permsArray);
        });
    }

    /**
     * Set a new value for an existing permission.
     *
     * @param perm A {@link ContentPermission} that you wish to update the value of.
     * @param value The new value for the permission.
     */
    @AnyThread
    public void setPermission(final @NonNull ContentPermission perm, final @ContentPermission.Value int value) {
        if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_TRACKING &&
                value == ContentPermission.VALUE_PROMPT) {
            Log.w(LOGTAG, "Cannot set a tracking permission to VALUE_PROMPT, aborting.");
            return;
        }
        final GeckoBundle msg = perm.toGeckoBundle();
        msg.putInt("newValue", value);
        EventDispatcher.getInstance().dispatch("GeckoView:SetPermission", msg);
    }

    /**
     * Add or modify a permission for a URI given by String. Assumes default context ID and
     * regular (non-private) browsing mode.
     *
     * @param uri A String representing the URI for which you are adding/modifying permissions.
     * @param type An int representing the permission you wish to add/modify.
     * @param value The new value for the permission.
     */
    @AnyThread
    @DeprecationSchedule(id = "setpermission-string", version = 93)
    public void setPermission(final @NonNull String uri, final int type, final @ContentPermission.Value int value) {
        setPermission(uri, null, false, type, value);
    }

    /**
     * Add or modify a permission for a URI given by String.
     *
     * @param uri A String representing the URI for which you are adding/modifying permissions.
     * @param contextId A String specifying the context ID under which the permission will apply.
     * @param privateMode A boolean indicating whether this permission should apply in private mode or normal mode.
     * @param type An int representing the permission you wish to add/modify.
     * @param value The new value for the permission.
     */
    @AnyThread
    @DeprecationSchedule(id = "setpermission-string", version = 93)
    public void setPermission(final @NonNull String uri, final @Nullable String contextId, final boolean privateMode, final int type, final @ContentPermission.Value int value) {
        if (type < GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION || type > GeckoSession.PermissionDelegate.PERMISSION_TRACKING) {
            Log.w(LOGTAG, "Invalid permission, aborting.");
            return;
        }
        if (type == GeckoSession.PermissionDelegate.PERMISSION_TRACKING && value == ContentPermission.VALUE_PROMPT) {
            Log.w(LOGTAG, "Cannot set a tracking permission to VALUE_PROMPT, aborting.");
            return;
        }
        final GeckoBundle msg = new GeckoBundle(5);
        msg.putString("uri", uri);
        msg.putString("contextId", createSafeSessionContextId(contextId));
        msg.putString("perm", GeckoSession.PermissionDelegate.ContentPermission.convertType(type, privateMode));
        msg.putInt("newValue", value);
        msg.putInt("privateId", privateMode ? 1 : 0);
        EventDispatcher.getInstance().dispatch("GeckoView:SetPermissionByURI", msg);
    }
}
