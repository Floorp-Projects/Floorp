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
}
