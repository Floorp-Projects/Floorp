/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.LongDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.util.List;
import java.util.Locale;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission;

/**
 * Manage runtime storage data.
 *
 * <p>Retrieve an instance via {@link GeckoRuntime#getStorageController}.
 */
public final class StorageController {
  private static final String LOGTAG = "StorageController";

  // Keep in sync with GeckoViewStorageController.ClearFlags.
  /** Flags used for data clearing operations. */
  public static class ClearFlags {
    /** Cookies. */
    public static final long COOKIES = 1 << 0;

    /** Network cache. */
    public static final long NETWORK_CACHE = 1 << 1;

    /** Image cache. */
    public static final long IMAGE_CACHE = 1 << 2;

    /** DOM storages. */
    public static final long DOM_STORAGES = 1 << 4;

    /** Auth tokens and caches. */
    public static final long AUTH_SESSIONS = 1 << 5;

    /** Site permissions. */
    public static final long PERMISSIONS = 1 << 6;

    /** All caches. */
    public static final long ALL_CACHES = NETWORK_CACHE | IMAGE_CACHE;

    /** All site settings (permissions, content preferences, security settings, etc.). */
    public static final long SITE_SETTINGS = 1 << 7 | PERMISSIONS;

    /** All site-related data (cookies, storages, caches, permissions, etc.). */
    public static final long SITE_DATA =
        1 << 8 | COOKIES | DOM_STORAGES | ALL_CACHES | PERMISSIONS | SITE_SETTINGS;

    /** All data. */
    public static final long ALL = 1 << 9;
  }

  @Retention(RetentionPolicy.SOURCE)
  @LongDef(
      flag = true,
      value = {
        ClearFlags.COOKIES,
        ClearFlags.NETWORK_CACHE,
        ClearFlags.IMAGE_CACHE,
        ClearFlags.DOM_STORAGES,
        ClearFlags.AUTH_SESSIONS,
        ClearFlags.PERMISSIONS,
        ClearFlags.ALL_CACHES,
        ClearFlags.SITE_SETTINGS,
        ClearFlags.SITE_DATA,
        ClearFlags.ALL
      })
  public @interface StorageControllerClearFlags {}

  /**
   * Clear data for all hosts.
   *
   * <p>Note: Any open session may re-accumulate previously cleared data. To ensure that no
   * persistent data is left behind, you need to close all sessions prior to clearing data.
   *
   * @param flags Combination of {@link ClearFlags}.
   * @return A {@link GeckoResult} that will complete when clearing has finished.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> clearData(final @StorageControllerClearFlags long flags) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putLong("flags", flags);

    return EventDispatcher.getInstance().queryVoid("GeckoView:ClearData", bundle);
  }

  /**
   * Clear data owned by the given host. Clearing data for a host will not clear data created by its
   * third-party origins.
   *
   * <p>Note: Any open session may re-accumulate previously cleared data. To ensure that no
   * persistent data is left behind, you need to close all sessions prior to clearing data.
   *
   * @param host The host to be used.
   * @param flags Combination of {@link ClearFlags}.
   * @return A {@link GeckoResult} that will complete when clearing has finished.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> clearDataFromHost(
      final @NonNull String host, final @StorageControllerClearFlags long flags) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("host", host);
    bundle.putLong("flags", flags);

    return EventDispatcher.getInstance().queryVoid("GeckoView:ClearHostData", bundle);
  }

  /**
   * Clear data owned by the given base domain (eTLD+1). Clearing data for a base domain will also
   * clear any associated third-party storage. This includes clearing for third-parties embedded by
   * the domain and for the given domain embedded under other sites.
   *
   * <p>Note: Any open session may re-accumulate previously cleared data. To ensure that no
   * persistent data is left behind, you need to close all sessions prior to clearing data.
   *
   * @param baseDomain The base domain to be used.
   * @param flags Combination of {@link ClearFlags}.
   * @return A {@link GeckoResult} that will complete when clearing has finished.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> clearDataFromBaseDomain(
      final @NonNull String baseDomain, final @StorageControllerClearFlags long flags) {
    final GeckoBundle bundle = new GeckoBundle(2);
    bundle.putString("baseDomain", baseDomain);
    bundle.putLong("flags", flags);

    return EventDispatcher.getInstance().queryVoid("GeckoView:ClearBaseDomainData", bundle);
  }

  /**
   * Clear data for the given context ID. Use {@link GeckoSessionSettings.Builder#contextId}.to set
   * a context ID for a session.
   *
   * <p>Note: Any open session may re-accumulate previously cleared data. To ensure that no
   * persistent data is left behind, you need to close all sessions for the given context prior to
   * clearing data.
   *
   * @param contextId The context ID for the storage data to be deleted.
   */
  @AnyThread
  public void clearDataForSessionContext(final @NonNull String contextId) {
    final GeckoBundle bundle = new GeckoBundle(1);
    bundle.putString("contextId", createSafeSessionContextId(contextId));

    EventDispatcher.getInstance().dispatch("GeckoView:ClearSessionContextData", bundle);
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
    return String.format("gvctx%x", new BigInteger(contextId.getBytes())).toLowerCase(Locale.ROOT);
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
   * @return A {@link GeckoResult} that will complete with a list of all currently stored {@link
   *     ContentPermission}s.
   */
  @AnyThread
  public @NonNull GeckoResult<List<ContentPermission>> getAllPermissions() {
    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:GetAllPermissions")
        .map(
            bundle -> {
              final GeckoBundle[] permsArray = bundle.getBundleArray("permissions");
              return ContentPermission.fromBundleArray(permsArray);
            });
  }

  /**
   * Get all currently stored permissions for a given URI and default (unset) context ID, in normal
   * mode This API will be deprecated in the future
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1797379
   *
   * @param uri A String representing the URI to get permissions for.
   * @return A {@link GeckoResult} that will complete with a list of all currently stored {@link
   *     ContentPermission}s for the URI.
   */
  @AnyThread
  public @NonNull GeckoResult<List<ContentPermission>> getPermissions(final @NonNull String uri) {
    return getPermissions(uri, null, false);
  }

  /**
   * Get all currently stored permissions for a given URI and default (unset) context ID.
   *
   * @param uri A String representing the URI to get permissions for.
   * @param privateMode indicate where the {@link ContentPermission}s should be in private or normal
   *     mode.
   * @return A {@link GeckoResult} that will complete with a list of all currently stored {@link
   *     ContentPermission}s for the URI.
   */
  @AnyThread
  public @NonNull GeckoResult<List<ContentPermission>> getPermissions(
      final @NonNull String uri, final boolean privateMode) {
    return getPermissions(uri, null, privateMode);
  }

  /**
   * Get all currently stored permissions for a given URI and context ID.
   *
   * @param uri A String representing the URI to get permissions for.
   * @param contextId A String specifying the context ID.
   * @param privateMode indicate where the {@link ContentPermission}s should be in private or normal
   *     mode
   * @return A {@link GeckoResult} that will complete with a list of all currently stored {@link
   *     ContentPermission}s for the URI.
   */
  @AnyThread
  public @NonNull GeckoResult<List<ContentPermission>> getPermissions(
      final @NonNull String uri, final @Nullable String contextId, final boolean privateMode) {
    final GeckoBundle msg = new GeckoBundle(2);
    final int privateBrowsingId = (privateMode) ? 1 : 0;
    msg.putString("uri", uri);
    msg.putString("contextId", createSafeSessionContextId(contextId));
    msg.putInt("privateBrowsingId", privateBrowsingId);
    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:GetPermissionsByURI", msg)
        .map(
            bundle -> {
              final GeckoBundle[] permsArray = bundle.getBundleArray("permissions");
              return ContentPermission.fromBundleArray(permsArray);
            });
  }

  /**
   * Set a new value for an existing permission.
   *
   * <p>Note: in private browsing, this value will only be cleared at the end of the session to add
   * permanent permissions in private browsing, you can use {@link
   * #setPrivateBrowsingPermanentPermission}.
   *
   * @param perm A {@link ContentPermission} that you wish to update the value of.
   * @param value The new value for the permission.
   */
  @AnyThread
  public void setPermission(
      final @NonNull ContentPermission perm, final @ContentPermission.Value int value) {
    setPermissionInternal(perm, value, /* allowPermanentPrivateBrowsing */ false);
  }

  /**
   * Set a permanent value for a permission in a private browsing session.
   *
   * <p>Normally permissions in private browsing are cleared at the end of the session. This method
   * allows you to set a permanent permission bypassing this behavior.
   *
   * <p>Note: permanent permissions in private browsing are web discoverable and might make the user
   * more easily trackable.
   *
   * @see #setPermission
   * @param perm A {@link ContentPermission} that you wish to update the value of.
   * @param value The new value for the permission.
   */
  @AnyThread
  public void setPrivateBrowsingPermanentPermission(
      final @NonNull ContentPermission perm, final @ContentPermission.Value int value) {
    setPermissionInternal(perm, value, /* allowPermanentPrivateBrowsing */ true);
  }

  private void setPermissionInternal(
      final @NonNull ContentPermission perm,
      final @ContentPermission.Value int value,
      final boolean allowPermanentPrivateBrowsing) {
    if (perm.permission == GeckoSession.PermissionDelegate.PERMISSION_TRACKING
        && value == ContentPermission.VALUE_PROMPT) {
      Log.w(LOGTAG, "Cannot set a tracking permission to VALUE_PROMPT, aborting.");
      return;
    }
    final GeckoBundle msg = perm.toGeckoBundle();
    msg.putInt("newValue", value);
    msg.putBoolean("allowPermanentPrivateBrowsing", allowPermanentPrivateBrowsing);
    EventDispatcher.getInstance().dispatch("GeckoView:SetPermission", msg);
  }

  /**
   * Set a permanent {@link ContentBlocking.CBCookieBannerMode} for the given uri and browsing mode.
   *
   * @param uri An uri for which you want change the {@link ContentBlocking.CBCookieBannerMode}
   *     value.
   * @param mode A new {@link ContentBlocking.CBCookieBannerMode} for the given uri.
   * @param isPrivateBrowsing Indicates in which browsing mode the given {@link
   *     ContentBlocking.CBCookieBannerMode} should be applied.
   * @return A {@link GeckoResult} that will complete when the mode has been set.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> setCookieBannerModeForDomain(
      final @NonNull String uri,
      final @ContentBlocking.CBCookieBannerMode int mode,
      final boolean isPrivateBrowsing) {
    final GeckoBundle data = new GeckoBundle(3);
    data.putString("uri", uri);
    data.putInt("mode", mode);
    data.putBoolean("allowPermanentPrivateBrowsing", false);
    data.putBoolean("isPrivateBrowsing", isPrivateBrowsing);
    return EventDispatcher.getInstance().queryVoid("GeckoView:SetCookieBannerModeForDomain", data);
  }

  /**
   * Set a permanent {@link ContentBlocking.CBCookieBannerMode} for the given uri in private mode.
   *
   * @param uri for which you want to change the {@link ContentBlocking.CBCookieBannerMode} value.
   * @param mode A new {@link ContentBlocking.CBCookieBannerMode} for the given uri.
   * @return A {@link GeckoResult} that will complete when the mode has been set.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> setCookieBannerModeAndPersistInPrivateBrowsingForDomain(
      final @NonNull String uri, final @ContentBlocking.CBCookieBannerMode int mode) {
    final GeckoBundle data = new GeckoBundle(3);
    data.putString("uri", uri);
    data.putInt("mode", mode);
    data.putBoolean("allowPermanentPrivateBrowsing", true);
    return EventDispatcher.getInstance().queryVoid("GeckoView:SetCookieBannerModeForDomain", data);
  }

  /**
   * Removes a {@link ContentBlocking.CBCookieBannerMode} for the given uri and and browsing mode.
   *
   * @param uri An uri for which you want change the {@link ContentBlocking.CBCookieBannerMode}
   *     value.
   * @param isPrivateBrowsing Indicates in which mode the given mode should be applied.
   * @return A {@link GeckoResult} that will complete when the mode has been removed.
   */
  @AnyThread
  public @NonNull GeckoResult<Void> removeCookieBannerModeForDomain(
      final @NonNull String uri, final boolean isPrivateBrowsing) {

    final GeckoBundle data = new GeckoBundle(3);
    data.putString("uri", uri);
    data.putBoolean("isPrivateBrowsing", isPrivateBrowsing);
    return EventDispatcher.getInstance()
        .queryVoid("GeckoView:RemoveCookieBannerModeForDomain", data);
  }

  /**
   * Gets the actual {@link ContentBlocking.CBCookieBannerMode} for the given uri and browsing mode.
   *
   * @param uri An uri for which you want get the {@link ContentBlocking.CBCookieBannerMode}.
   * @param isPrivateBrowsing Indicates in which browsing mode the given uri should be.
   * @return A {@link GeckoResult} that resolves to a {@link ContentBlocking.CBCookieBannerMode} for
   *     the given uri and browsing mode.
   */
  @AnyThread
  public @NonNull @ContentBlocking.CBCookieBannerMode GeckoResult<Integer>
      getCookieBannerModeForDomain(final @NonNull String uri, final boolean isPrivateBrowsing) {

    final GeckoBundle data = new GeckoBundle(2);
    data.putString("uri", uri);
    data.putBoolean("isPrivateBrowsing", isPrivateBrowsing);
    return EventDispatcher.getInstance()
        .queryBundle("GeckoView:GetCookieBannerModeForDomain", data)
        .map(StorageController::cookieBannerModeFromBundle, StorageController::fromQueryException);
  }

  private static @ContentBlocking.CBCookieBannerMode int cookieBannerModeFromBundle(
      final GeckoBundle bundle) throws Exception {
    if (bundle == null) {
      throw new Exception("Unable to parse cookie banner mode");
    }
    return bundle.getInt("mode");
  }

  private static Throwable fromQueryException(final Throwable exception) {
    final EventDispatcher.QueryException queryException =
        (EventDispatcher.QueryException) exception;
    final Object response = queryException.data;
    return new Exception(response.toString());
  }
}
