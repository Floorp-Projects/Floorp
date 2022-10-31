/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage.serialize

internal const val VERSION = 2

/**
 * JSON keys used in state getting read/written by [BrowserStateReader] and [BrowserStateWriter].
 */
internal object Keys {
    const val SELECTED_SESSION_INDEX_KEY = "selectedSessionIndex"
    const val SELECTED_TAB_ID_KEY = "selectedTabId"
    const val SESSION_STATE_TUPLES_KEY = "sessionStateTuples"

    const val SESSION_URL_KEY = "url"
    const val SESSION_UUID_KEY = "uuid"
    const val SESSION_CONTEXT_ID_KEY = "contextId"
    const val SESSION_PARENT_UUID_KEY = "parentUuid"
    const val SESSION_READER_MODE_KEY = "readerMode"
    const val SESSION_READER_MODE_ACTIVE_URL_KEY = "readerModeArticleUrl"
    const val SESSION_TITLE = "title"
    const val SESSION_SEARCH_TERM = "searchTerm"
    const val SESSION_LAST_ACCESS = "lastAccess"
    const val SESSION_CREATED_AT = "createdAt"
    const val SESSION_LAST_MEDIA_URL = "lastMediaUrl"
    const val SESSION_LAST_MEDIA_ACCESS = "lastMediaAccess"
    const val SESSION_LAST_MEDIA_SESSION_ACTIVE = "mediaSessionActive"

    // Deprecated for SESSION_SOURCE_ID, kept around for backwards compatibility.
    const val SESSION_DEPRECATED_SOURCE_KEY = "source"

    const val SESSION_HISTORY_METADATA_URL = "historyMetadataUrl"
    const val SESSION_HISTORY_METADATA_SEARCH_TERM = "historyMetadataSearchTerm"
    const val SESSION_HISTORY_METADATA_REFERRER_URL = "historyMetadataReferrerUrl"

    const val SESSION_SOURCE_ID = "sourceId"
    const val SESSION_EXTERNAL_SOURCE_PACKAGE_ID = "externalPackageId"
    const val SESSION_EXTERNAL_SOURCE_PACKAGE_CATEGORY = "externalPackageCategory"

    const val SESSION_KEY = "session"
    const val ENGINE_SESSION_KEY = "engineSession"

    const val VERSION_KEY = "version"
}
