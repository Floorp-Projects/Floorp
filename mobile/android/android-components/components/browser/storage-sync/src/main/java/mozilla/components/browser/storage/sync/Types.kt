/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")

package mozilla.components.browser.storage.sync

import mozilla.appservices.places.SyncAuthInfo
import mozilla.appservices.places.uniffi.BookmarkItem
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.DocumentType
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.storage.HistoryHighlight
import mozilla.components.concept.storage.HistoryHighlightWeights
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.concept.storage.HistoryMetadataObservation
import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType

// We have type definitions at the concept level, and "external" types defined within Places.
// In practice these two types are largely the same, and this file is the conversion point.

/**
 * Conversion from our SyncAuthInfo into its "native" version used at the interface boundary.
 */
internal fun mozilla.components.concept.sync.SyncAuthInfo.into(): SyncAuthInfo {
    return SyncAuthInfo(
        kid = this.kid,
        fxaAccessToken = this.fxaAccessToken,
        syncKey = this.syncKey,
        tokenserverURL = this.tokenServerUrl,
    )
}

/**
 * Conversion from a generic [FrecencyThresholdOption] into its richer comrade within the 'places' lib.
 */
internal fun FrecencyThresholdOption.into() = when (this) {
    FrecencyThresholdOption.NONE -> mozilla.appservices.places.uniffi.FrecencyThresholdOption.NONE
    FrecencyThresholdOption.SKIP_ONE_TIME_PAGES ->
        mozilla.appservices.places.uniffi.FrecencyThresholdOption.SKIP_ONE_TIME_PAGES
}

/**
 * Conversion from a generic [VisitType] into its richer comrade within the 'places' lib.
 */
internal fun VisitType.into() = when (this) {
    VisitType.NOT_A_VISIT -> mozilla.appservices.places.VisitType.UPDATE_PLACE
    VisitType.LINK -> mozilla.appservices.places.VisitType.LINK
    VisitType.RELOAD -> mozilla.appservices.places.VisitType.RELOAD
    VisitType.TYPED -> mozilla.appservices.places.VisitType.TYPED
    VisitType.BOOKMARK -> mozilla.appservices.places.VisitType.BOOKMARK
    VisitType.EMBED -> mozilla.appservices.places.VisitType.EMBED
    VisitType.REDIRECT_PERMANENT -> mozilla.appservices.places.VisitType.REDIRECT_PERMANENT
    VisitType.REDIRECT_TEMPORARY -> mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY
    VisitType.DOWNLOAD -> mozilla.appservices.places.VisitType.DOWNLOAD
    VisitType.FRAMED_LINK -> mozilla.appservices.places.VisitType.FRAMED_LINK
}

internal fun mozilla.appservices.places.VisitType.into() = when (this) {
    mozilla.appservices.places.VisitType.UPDATE_PLACE -> VisitType.NOT_A_VISIT
    mozilla.appservices.places.VisitType.LINK -> VisitType.LINK
    mozilla.appservices.places.VisitType.RELOAD -> VisitType.RELOAD
    mozilla.appservices.places.VisitType.TYPED -> VisitType.TYPED
    mozilla.appservices.places.VisitType.BOOKMARK -> VisitType.BOOKMARK
    mozilla.appservices.places.VisitType.EMBED -> VisitType.EMBED
    mozilla.appservices.places.VisitType.REDIRECT_PERMANENT -> VisitType.REDIRECT_PERMANENT
    mozilla.appservices.places.VisitType.REDIRECT_TEMPORARY -> VisitType.REDIRECT_TEMPORARY
    mozilla.appservices.places.VisitType.DOWNLOAD -> VisitType.DOWNLOAD
    mozilla.appservices.places.VisitType.FRAMED_LINK -> VisitType.FRAMED_LINK
}

internal fun VisitType.intoTransitionType() = when (this) {
    VisitType.NOT_A_VISIT -> null
    VisitType.LINK -> mozilla.appservices.places.uniffi.VisitTransition.LINK
    VisitType.RELOAD -> mozilla.appservices.places.uniffi.VisitTransition.RELOAD
    VisitType.TYPED -> mozilla.appservices.places.uniffi.VisitTransition.TYPED
    VisitType.BOOKMARK -> mozilla.appservices.places.uniffi.VisitTransition.BOOKMARK
    VisitType.EMBED -> mozilla.appservices.places.uniffi.VisitTransition.EMBED
    VisitType.REDIRECT_PERMANENT -> mozilla.appservices.places.uniffi.VisitTransition.REDIRECT_PERMANENT
    VisitType.REDIRECT_TEMPORARY -> mozilla.appservices.places.uniffi.VisitTransition.REDIRECT_TEMPORARY
    VisitType.DOWNLOAD -> mozilla.appservices.places.uniffi.VisitTransition.DOWNLOAD
    VisitType.FRAMED_LINK -> mozilla.appservices.places.uniffi.VisitTransition.FRAMED_LINK
}

internal fun mozilla.appservices.places.uniffi.VisitTransition.into() = when (this) {
    mozilla.appservices.places.uniffi.VisitTransition.LINK -> VisitType.LINK
    mozilla.appservices.places.uniffi.VisitTransition.RELOAD -> VisitType.RELOAD
    mozilla.appservices.places.uniffi.VisitTransition.TYPED -> VisitType.TYPED
    mozilla.appservices.places.uniffi.VisitTransition.BOOKMARK -> VisitType.BOOKMARK
    mozilla.appservices.places.uniffi.VisitTransition.EMBED -> VisitType.EMBED
    mozilla.appservices.places.uniffi.VisitTransition.REDIRECT_PERMANENT -> VisitType.REDIRECT_PERMANENT
    mozilla.appservices.places.uniffi.VisitTransition.REDIRECT_TEMPORARY -> VisitType.REDIRECT_TEMPORARY
    mozilla.appservices.places.uniffi.VisitTransition.DOWNLOAD -> VisitType.DOWNLOAD
    mozilla.appservices.places.uniffi.VisitTransition.FRAMED_LINK -> VisitType.FRAMED_LINK
}

internal fun mozilla.appservices.places.uniffi.HistoryVisitInfo.into(): VisitInfo {
    return VisitInfo(
        url = this.url,
        title = this.title,
        visitTime = this.timestamp,
        visitType = this.visitType.into(),
        previewImageUrl = this.previewImageUrl,
        isRemote = this.isRemote,
    )
}

internal fun mozilla.appservices.places.uniffi.TopFrecentSiteInfo.into(): TopFrecentSiteInfo {
    return TopFrecentSiteInfo(
        url = this.url,
        title = this.title,
    )
}

internal fun BookmarkItem.asBookmarkNode(): BookmarkNode {
    return when (this) {
        is BookmarkItem.Bookmark -> {
            BookmarkNode(
                BookmarkNodeType.ITEM,
                this.b.guid,
                this.b.parentGuid,
                this.b.position,
                this.b.title,
                this.b.url,
                this.b.dateAdded,
                null,
            )
        }
        is BookmarkItem.Folder -> {
            BookmarkNode(
                BookmarkNodeType.FOLDER,
                this.f.guid,
                this.f.parentGuid,
                this.f.position,
                this.f.title,
                null,
                this.f.dateAdded,
                this.f.childNodes?.map(BookmarkItem::asBookmarkNode),
            )
        }
        is BookmarkItem.Separator -> {
            BookmarkNode(
                BookmarkNodeType.SEPARATOR,
                this.s.guid,
                this.s.parentGuid,
                this.s.position,
                null,
                null,
                this.s.dateAdded,
                null,
            )
        }
    }
}

internal fun HistoryMetadataKey.into(): mozilla.appservices.places.HistoryMetadataKey {
    return mozilla.appservices.places.HistoryMetadataKey(
        url = this.url,
        referrerUrl = this.referrerUrl,
        searchTerm = this.searchTerm,
    )
}

internal fun mozilla.appservices.places.HistoryMetadataKey.into(): HistoryMetadataKey {
    return HistoryMetadataKey(
        url = this.url,
        referrerUrl = if (this.referrerUrl.isNullOrEmpty()) { null } else { this.referrerUrl },
        searchTerm = if (this.searchTerm.isNullOrEmpty()) { null } else { this.searchTerm },
    )
}

internal fun mozilla.appservices.places.uniffi.DocumentType.into(): DocumentType {
    return when (this) {
        mozilla.appservices.places.uniffi.DocumentType.REGULAR -> DocumentType.Regular
        mozilla.appservices.places.uniffi.DocumentType.MEDIA -> DocumentType.Media
    }
}

internal fun mozilla.appservices.places.uniffi.HistoryMetadata.into(): HistoryMetadata {
    // Protobuf doesn't support passing around `null` value, so these get converted to some defaults
    // as they go from Rust to Kotlin. E.g. an empty string in place of a `null`.
    // That means places.HistoryMetadata will never have `null` values.
    // But, we actually do want a real `null` value here - hence the explicit check.
    return HistoryMetadata(
        key = HistoryMetadataKey(url = this.url, searchTerm = this.searchTerm, referrerUrl = this.referrerUrl),
        title = if (this.title.isNullOrEmpty()) null else this.title,
        createdAt = this.createdAt,
        updatedAt = this.updatedAt,
        totalViewTime = this.totalViewTime,
        documentType = this.documentType.into(),
        previewImageUrl = this.previewImageUrl,
    )
}

internal fun List<mozilla.appservices.places.uniffi.HistoryMetadata>.into(): List<HistoryMetadata> {
    return map { it.into() }
}

internal fun mozilla.appservices.places.uniffi.HistoryHighlight.into(): HistoryHighlight {
    return HistoryHighlight(
        score = this.score,
        placeId = this.placeId,
        url = this.url,
        title = this.title,
        previewImageUrl = this.previewImageUrl,
    )
}

internal fun List<mozilla.appservices.places.uniffi.HistoryHighlight>.intoHighlights(): List<HistoryHighlight> {
    return map { it.into() }
}

internal fun HistoryHighlightWeights.into(): mozilla.appservices.places.uniffi.HistoryHighlightWeights {
    return mozilla.appservices.places.uniffi.HistoryHighlightWeights(
        viewTime = this.viewTime,
        frequency = this.frequency,
    )
}

internal fun DocumentType.into(): mozilla.appservices.places.uniffi.DocumentType {
    return when (this) {
        DocumentType.Regular -> mozilla.appservices.places.uniffi.DocumentType.REGULAR
        DocumentType.Media -> mozilla.appservices.places.uniffi.DocumentType.MEDIA
    }
}

internal fun HistoryMetadata.into(): mozilla.appservices.places.uniffi.HistoryMetadata {
    return mozilla.appservices.places.uniffi.HistoryMetadata(
        url = this.key.url,
        searchTerm = this.key.searchTerm,
        referrerUrl = this.key.referrerUrl,
        title = this.title,
        createdAt = this.createdAt,
        updatedAt = this.updatedAt,
        totalViewTime = this.totalViewTime,
        documentType = this.documentType.into(),
        previewImageUrl = this.previewImageUrl,
    )
}

internal fun HistoryMetadataObservation.into(
    key: HistoryMetadataKey,
): mozilla.appservices.places.uniffi.HistoryMetadataObservation {
    return when (this) {
        is HistoryMetadataObservation.ViewTimeObservation -> {
            mozilla.appservices.places.uniffi.HistoryMetadataObservation(
                url = key.url,
                searchTerm = key.searchTerm,
                referrerUrl = key.referrerUrl,
                viewTime = this.viewTime,
            )
        }
        is HistoryMetadataObservation.DocumentTypeObservation -> {
            mozilla.appservices.places.uniffi.HistoryMetadataObservation(
                url = key.url,
                searchTerm = key.searchTerm,
                referrerUrl = key.referrerUrl,
                documentType = this.documentType.into(),
            )
        }
    }
}
