/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.UiThread;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * ContentBlockingController is used to manage and modify the content blocking exception list. This
 * list is shared across all sessions.
 */
@AnyThread
public class ContentBlockingController {
  private static final String LOGTAG = "GeckoContentBlocking";

  public static class Event {
    // These values must be kept in sync with the corresponding values in
    // nsIWebProgressListener.idl.
    /** Tracking content has been blocked from loading. */
    public static final int BLOCKED_TRACKING_CONTENT = 0x00001000;

    /** Level 1 tracking content has been loaded. */
    public static final int LOADED_LEVEL_1_TRACKING_CONTENT = 0x00002000;

    /** Level 2 tracking content has been loaded. */
    public static final int LOADED_LEVEL_2_TRACKING_CONTENT = 0x00100000;

    /** Fingerprinting content has been blocked from loading. */
    public static final int BLOCKED_FINGERPRINTING_CONTENT = 0x00000040;

    /** Fingerprinting content has been loaded. */
    public static final int LOADED_FINGERPRINTING_CONTENT = 0x00000400;

    /** Cryptomining content has been blocked from loading. */
    public static final int BLOCKED_CRYPTOMINING_CONTENT = 0x00000800;

    /** Cryptomining content has been loaded. */
    public static final int LOADED_CRYPTOMINING_CONTENT = 0x00200000;

    /** Content which appears on the SafeBrowsing list has been blocked from loading. */
    public static final int BLOCKED_UNSAFE_CONTENT = 0x00004000;

    /**
     * Performed a storage access check, which usually means something like a cookie or a storage
     * item was loaded/stored on the current tab. Alternatively this could indicate that something
     * in the current tab attempted to communicate with its same-origin counterparts in other tabs.
     */
    public static final int COOKIES_LOADED = 0x00008000;

    /**
     * Similar to {@link #COOKIES_LOADED}, but only sent if the subject of the action was a
     * third-party tracker when the active cookie policy imposes restrictions on such content.
     */
    public static final int COOKIES_LOADED_TRACKER = 0x00040000;

    /**
     * Similar to {@link #COOKIES_LOADED}, but only sent if the subject of the action was a
     * third-party social tracker when the active cookie policy imposes restrictions on such
     * content.
     */
    public static final int COOKIES_LOADED_SOCIALTRACKER = 0x00080000;

    /** Rejected for custom site permission. */
    public static final int COOKIES_BLOCKED_BY_PERMISSION = 0x10000000;

    /** Rejected because the resource is a tracker and cookie policy doesn't allow its loading. */
    public static final int COOKIES_BLOCKED_TRACKER = 0x20000000;

    /**
     * Rejected because the resource is a tracker from a social origin and cookie policy doesn't
     * allow its loading.
     */
    public static final int COOKIES_BLOCKED_SOCIALTRACKER = 0x01000000;

    /** Rejected because cookie policy blocks all cookies. */
    public static final int COOKIES_BLOCKED_ALL = 0x40000000;

    /**
     * Rejected because the resource is a third-party and cookie policy forces third-party resources
     * to be partitioned.
     */
    public static final int COOKIES_PARTITIONED_FOREIGN = 0x80000000;

    /** Rejected because cookie policy blocks 3rd party cookies. */
    public static final int COOKIES_BLOCKED_FOREIGN = 0x00000080;

    /** SocialTracking content has been blocked from loading. */
    public static final int BLOCKED_SOCIALTRACKING_CONTENT = 0x00010000;

    /** SocialTracking content has been loaded. */
    public static final int LOADED_SOCIALTRACKING_CONTENT = 0x00020000;

    /** Email content has been blocked from loading. */
    public static final int BLOCKED_EMAILTRACKING_CONTENT = 0x00400000;

    /** EmailTracking content from the Disconnect level 1 has been loaded. */
    public static final int LOADED_EMAILTRACKING_LEVEL_1_CONTENT = 0x00800000;

    /** EmailTracking content from the Disconnect level 2 has been loaded. */
    public static final int LOADED_EMAILTRACKING_LEVEL_2_CONTENT = 0x00000100;

    /**
     * Indicates that content that would have been blocked has instead been replaced with a shim.
     */
    public static final int REPLACED_TRACKING_CONTENT = 0x00000010;

    /** Indicates that content that would have been blocked has instead been allowed by a shim. */
    public static final int ALLOWED_TRACKING_CONTENT = 0x00000020;

    protected Event() {}
  }

  /** An entry in the content blocking log for a site. */
  @AnyThread
  public static class LogEntry {
    /** Data about why a given entry was blocked. */
    public static class BlockingData {
      @Retention(RetentionPolicy.SOURCE)
      @IntDef({
        Event.BLOCKED_TRACKING_CONTENT, Event.LOADED_LEVEL_1_TRACKING_CONTENT,
        Event.LOADED_LEVEL_2_TRACKING_CONTENT, Event.BLOCKED_FINGERPRINTING_CONTENT,
        Event.LOADED_FINGERPRINTING_CONTENT, Event.BLOCKED_CRYPTOMINING_CONTENT,
        Event.LOADED_CRYPTOMINING_CONTENT, Event.BLOCKED_UNSAFE_CONTENT,
        Event.COOKIES_LOADED, Event.COOKIES_LOADED_TRACKER,
        Event.COOKIES_LOADED_SOCIALTRACKER, Event.COOKIES_BLOCKED_BY_PERMISSION,
        Event.COOKIES_BLOCKED_TRACKER, Event.COOKIES_BLOCKED_SOCIALTRACKER,
        Event.COOKIES_BLOCKED_ALL, Event.COOKIES_PARTITIONED_FOREIGN,
        Event.COOKIES_BLOCKED_FOREIGN, Event.BLOCKED_SOCIALTRACKING_CONTENT,
        Event.LOADED_SOCIALTRACKING_CONTENT, Event.REPLACED_TRACKING_CONTENT,
        Event.LOADED_EMAILTRACKING_LEVEL_1_CONTENT, Event.LOADED_EMAILTRACKING_LEVEL_2_CONTENT,
        Event.BLOCKED_EMAILTRACKING_CONTENT
      })
      public @interface LogEvent {}

      /** A category the entry falls under. */
      public final @LogEvent int category;

      /** Indicates whether or not blocking occured for this category, where applicable. */
      public final boolean blocked;

      /** The count of consecutive repeated appearances. */
      public final int count;

      /* package */ BlockingData(final @NonNull GeckoBundle bundle) {
        category = bundle.getInt("category");
        blocked = bundle.getBoolean("blocked");
        count = bundle.getInt("count");
      }

      protected BlockingData() {
        category = Event.BLOCKED_TRACKING_CONTENT;
        blocked = false;
        count = 0;
      }
    }

    /** The origin of this log entry. */
    public final @NonNull String origin;

    /** The blocking data for this origin, sorted chronologically. */
    public final @NonNull List<BlockingData> blockingData;

    /* package */ LogEntry(final @NonNull GeckoBundle bundle) {
      origin = bundle.getString("origin");
      final GeckoBundle[] data = bundle.getBundleArray("blockData");
      final ArrayList<BlockingData> dataArray = new ArrayList<BlockingData>(data.length);
      for (final GeckoBundle b : data) {
        dataArray.add(new BlockingData(b));
      }
      blockingData = Collections.unmodifiableList(dataArray);
    }

    protected LogEntry() {
      origin = null;
      blockingData = null;
    }
  }

  private List<LogEntry> logFromBundle(final GeckoBundle value) {
    final GeckoBundle[] bundles = value.getBundleArray("log");
    final ArrayList<LogEntry> logArray = new ArrayList<>(bundles.length);
    for (final GeckoBundle b : bundles) {
      logArray.add(new LogEntry(b));
    }
    return Collections.unmodifiableList(logArray);
  }

  /**
   * Get a log of all content blocking information for the site currently loaded by the supplied
   * {@link GeckoSession}.
   *
   * @param session A {@link GeckoSession} for which you want the content blocking log.
   * @return A {@link GeckoResult} that resolves to the list of content blocking log entries.
   */
  @UiThread
  public @NonNull GeckoResult<List<LogEntry>> getLog(final @NonNull GeckoSession session) {
    return session
        .getEventDispatcher()
        .queryBundle("ContentBlocking:RequestLog")
        .map(this::logFromBundle);
  }
}
