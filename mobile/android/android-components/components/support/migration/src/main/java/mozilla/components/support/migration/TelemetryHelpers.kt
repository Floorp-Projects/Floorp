/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

// Must match labels of the migration_versions metric.
internal fun Migration.telemetryIdentifier(): String {
    return when (this) {
        Migration.History -> "history"
        Migration.Bookmarks -> "bookmarks"
        Migration.OpenTabs -> "open_tabs"
        Migration.FxA -> "fxa"
        Migration.Gecko -> "gecko"
        Migration.Logins -> "logins"
        Migration.Settings -> "settings"
        Migration.Addons -> "addons"
        Migration.TelemetryIdentifiers -> "telemetry_identifiers"
        Migration.SearchEngine -> "search"
        Migration.PinnedSites -> "pinned_sites"
    }
}

// Failure/success codes are used in the migration ping, to avoid sending strings.
// Append any extra errors at the end with a unique, sequential (to make it easy to spot dupes) 'code'.
@SuppressWarnings("MagicNumber")
internal enum class FailureReasonTelemetryCodes(val code: Int) {
    LOGINS_MP_CHECK(1),
    LOGINS_UNSUPPORTED_LOGINS_DB(2),
    LOGINS_ENCRYPTION(3),
    LOGINS_GET(4),
    LOGINS_RUST_IMPORT(5),

    SETTINGS_MISSING_FHR_VALUE(6),
    SETTINGS_WRONG_TELEMETRY_VALUE(7),

    ADDON_QUERY(8),

    FXA_CORRUPT_ACCOUNT_STATE(9),
    FXA_UNSUPPORTED_VERSIONS(10),
    FXA_SIGN_IN_FAILED(11),
    FXA_CUSTOM_SERVER(12),

    HISTORY_MISSING_DB_PATH(13),
    HISTORY_RUST_EXCEPTION(14),
    HISTORY_TELEMETRY_EXCEPTION(15),

    BOOKMARKS_MISSING_DB_PATH(16),
    BOOKMARKS_RUST_EXCEPTION(17),
    BOOKMARKS_TELEMETRY_EXCEPTION(18),

    LOGINS_UNEXPECTED_EXCEPTION(19),
    LOGINS_MISSING_PROFILE(20),

    OPEN_TABS_MISSING_PROFILE(21),
    OPEN_TABS_MIGRATE_EXCEPTION(22),
    OPEN_TABS_NO_SNAPSHOT(23),
    OPEN_TABS_RESTORE_EXCEPTION(24),

    GECKO_MISSING_PROFILE(25),
    GECKO_UNEXPECTED_EXCEPTION(26),

    SETTINGS_MIGRATE_EXCEPTION(27),

    ADDON_UNEXPECTED_EXCEPTION(28),

    TELEMETRY_IDENTIFIERS_MISSING_PROFILE(29),
    TELEMETRY_IDENTIFIERS_MIGRATE_EXCEPTION(30),

    SEARCH_NO_DEFAULT(31),
    SEARCH_NO_MATCH(32),
    SEARCH_EXCEPTION(33),

    PINNED_SITES_MISSING_DB_PATH(34),
    PINNED_SITES_READ_FAILURE(35),
}

@SuppressWarnings("MagicNumber")
internal enum class SuccessReasonTelemetryCodes(val code: Int) {
    LOGINS_MP_SET(1),
    LOGINS_MIGRATED(2),

    FXA_NO_ACCOUNT(3),
    FXA_BAD_AUTH(4),
    FXA_SIGNED_IN(5),

    SETTINGS_NO_PREFS(6),
    SETTINGS_MIGRATED(7),

    ADDONS_NO(8),
    ADDONS_MIGRATED(9),

    HISTORY_NO_DB(10),
    HISTORY_MIGRATED(11),

    BOOKMARKS_NO_DB(12),
    BOOKMARKS_MIGRATED(13),

    OPEN_TABS_MIGRATED(14),

    GECKO_MIGRATED(15),

    TELEMETRY_IDENTIFIERS_MIGRATED(16),

    FXA_WILL_RETRY(17),
    SEARCH_MIGRATED(18),

    PINNED_SITES_NONE(19),
    PINNED_SITES_MIGRATED(20),
}
