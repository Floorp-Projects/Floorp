package org.mozilla.gecko.util;

import org.mozilla.gecko.R;

/**
 * (linter: UnusedResources) We use resources in places Android Lint can't check (e.g. JS) - this is
 * a set of those references so Android Lint stops complaining.
 */
@SuppressWarnings("unused")
final class UnusedResourcesUtil {
    public static final int[] CONSTANTS = {
            R.dimen.match_parent,
            R.dimen.wrap_content,
    };

    public static final int[] USED_IN_BRANDING = {
            R.drawable.large_icon
    };

    public static final int[] USED_IN_COLOR_PALETTE = {
            R.color.private_browsing_purple, // This will be used eventually, then this item removed.
    };

    public static final int[] USED_IN_CRASH_REPORTER = {
            R.string.crash_allow_contact2,
            R.string.crash_close_label,
            R.string.crash_comment,
            R.string.crash_email,
            R.string.crash_include_url2,
            R.string.crash_message2,
            R.string.crash_restart_label,
            R.string.crash_send_report_message3,
            R.string.crash_sorry,
    };

    public static final int[] USED_IN_JS = {
            R.drawable.ab_search,
            R.drawable.alert_camera,
            R.drawable.alert_download,
            R.drawable.alert_download_animation,
            R.drawable.alert_mic,
            R.drawable.alert_mic_camera,
            R.drawable.casting,
            R.drawable.casting_active,
            R.drawable.close,
            R.drawable.homepage_banner_firstrun,
            R.drawable.ic_readermode,
            R.drawable.ic_readermode_on,
            R.drawable.icon_openinapp,
            R.drawable.pause,
            R.drawable.phone,
            R.drawable.play,
            R.drawable.sync_promo,
            R.drawable.undo_button_icon,
    };

    public static final int[] USED_IN_MANIFEST = {
            R.string.crash_reporter_title,
            R.xml.fxaccount_authenticator,
            R.xml.fxaccount_syncadapter,
            R.xml.searchable,
    };

    public static final int[] USED_IN_SUGGESTEDSITES = {
            R.drawable.suggestedsites_amazon,
            R.drawable.suggestedsites_facebook,
            R.drawable.suggestedsites_restricted_fxsupport,
            R.drawable.suggestedsites_restricted_mozilla,
            R.drawable.suggestedsites_twitter,
            R.drawable.suggestedsites_webmaker,
            R.drawable.suggestedsites_wikipedia,
            R.drawable.suggestedsites_youtube,
    };

    public static final int[] USED_IN_BOOKMARKDEFAULTS = {
            R.raw.bookmarkdefaults_favicon_addons,
            R.raw.bookmarkdefaults_favicon_support,
            R.raw.bookmarkdefaults_favicon_restricted_support,
            R.raw.bookmarkdefaults_favicon_restricted_webmaker,
            R.string.bookmarkdefaults_title_restricted_support,
            R.string.bookmarkdefaults_url_restricted_support,
            R.string.bookmarkdefaults_title_restricted_webmaker,
            R.string.bookmarkdefaults_url_restricted_webmaker,
    };

    public static final int[] USED_IN_PREFS = {
            R.xml.preferences_advanced,
            R.xml.preferences_accessibility,
            R.xml.preferences_home,
            R.xml.preferences_privacy,
            R.xml.preferences_privacy_clear_tablet,
            R.xml.preferences_default_browser_tablet
    };

    // String resources that are used in the full-pane Activity Stream that are temporarily
    // not needed while Activity Stream is part of the HomePager
    public static final int[] TEMPORARY_UNUSED_ACTIVITY_STREAM = {
            R.string.activity_stream_topsites
    };

    public static final int[] USED_IN_PAGE_ACTION = {
            R.drawable.add_to_homescreen
    };

    public static final int[] USED_IN_LEANPLUM_EXPANDABLE_LIST_ACTIVITY = {
            R.style.Widget_ExpandableListView,
    };
}
