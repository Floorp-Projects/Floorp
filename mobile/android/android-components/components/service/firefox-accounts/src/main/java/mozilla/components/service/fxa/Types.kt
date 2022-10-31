/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:SuppressWarnings("MatchingDeclarationName")

package mozilla.components.service.fxa

import mozilla.appservices.fxaclient.AccessTokenInfo
import mozilla.appservices.fxaclient.AccountEvent
import mozilla.appservices.fxaclient.Device
import mozilla.appservices.fxaclient.IncomingDeviceCommand
import mozilla.appservices.fxaclient.MigrationState
import mozilla.appservices.fxaclient.Profile
import mozilla.appservices.fxaclient.ScopedKey
import mozilla.appservices.fxaclient.TabHistoryEntry
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.Avatar
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.InFlightMigrationState
import mozilla.components.concept.sync.OAuthScopedKey
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.appservices.fxaclient.DeviceCapability as RustDeviceCapability
import mozilla.appservices.fxaclient.DevicePushSubscription as RustDevicePushSubscription
import mozilla.appservices.fxaclient.DeviceType as RustDeviceType

/**
 * Converts a raw 'action' string into an [AuthType] instance.
 * Actions come to us from FxA during an OAuth login, either over the WebChannel or via the redirect URL.
 */
fun String?.toAuthType(): AuthType {
    return when (this) {
        "signin" -> AuthType.Signin
        "signup" -> AuthType.Signup
        "pairing" -> AuthType.Pairing
        // We want to gracefully handle an 'action' we don't know about.
        // This also covers the `null` case.
        else -> AuthType.OtherExternal(this)
    }
}

/**
 * Captures basic OAuth authentication data (code, state) and any additional data FxA passes along.
 * @property authType Type of authentication which caused this object to be created.
 * @property code OAuth code.
 * @property state OAuth state.
 * @property declinedEngines An optional list of [SyncEngine]s that user declined to sync.
 */
data class FxaAuthData(
    val authType: AuthType,
    val code: String,
    val state: String,
    val declinedEngines: Set<SyncEngine>? = null,
) {
    override fun toString(): String {
        return "authType: $authType, code: XXX, state: XXX, declinedEngines: $declinedEngines"
    }
}

// The rest of this file describes translations between fxaclient's internal type definitions and analogous
// types defined by concept-sync. It's a little tedious, but ensures decoupling between abstract
// definitions and a concrete implementation. In practice, this means that concept-sync doesn't need
// impose a dependency on fxaclient native library.

fun AccessTokenInfo.into(): mozilla.components.concept.sync.AccessTokenInfo {
    return mozilla.components.concept.sync.AccessTokenInfo(
        scope = this.scope,
        token = this.token,
        key = this.key?.into(),
        expiresAt = this.expiresAt,
    )
}

/**
 * Converts a generic [AccessTokenInfo] into a Firefox Sync-friendly [SyncAuthInfo] instance which
 * may be used for data synchronization.
 *
 * @return An [SyncAuthInfo] which is guaranteed to have a sync key.
 * @throws IllegalStateException if [AccessTokenInfo] didn't have key information.
 */
fun mozilla.components.concept.sync.AccessTokenInfo.asSyncAuthInfo(tokenServerUrl: String): SyncAuthInfo {
    val keyInfo = this.key ?: throw AccessTokenUnexpectedlyWithoutKey()

    return SyncAuthInfo(
        kid = keyInfo.kid,
        fxaAccessToken = this.token,
        fxaAccessTokenExpiresAt = this.expiresAt,
        syncKey = keyInfo.k,
        tokenServerUrl = tokenServerUrl,
    )
}

fun ScopedKey.into(): OAuthScopedKey {
    return OAuthScopedKey(kid = this.kid, k = this.k, kty = this.kty, scope = this.scope)
}

fun Profile.into(): mozilla.components.concept.sync.Profile {
    return mozilla.components.concept.sync.Profile(
        uid = this.uid,
        email = this.email,
        avatar = this.avatar.let {
            Avatar(
                url = it,
                isDefault = this.isDefaultAvatar,
            )
        },
        displayName = this.displayName,
    )
}

internal fun RustDeviceType.into(): DeviceType {
    return when (this) {
        RustDeviceType.DESKTOP -> DeviceType.DESKTOP
        RustDeviceType.MOBILE -> DeviceType.MOBILE
        RustDeviceType.TABLET -> DeviceType.TABLET
        RustDeviceType.TV -> DeviceType.TV
        RustDeviceType.VR -> DeviceType.VR
        RustDeviceType.UNKNOWN -> DeviceType.UNKNOWN
    }
}

/**
 * Convert between the native-code DeviceType data class
 * and the one from the corresponding a-c concept.
 */
fun DeviceType.into(): RustDeviceType {
    return when (this) {
        DeviceType.DESKTOP -> RustDeviceType.DESKTOP
        DeviceType.MOBILE -> RustDeviceType.MOBILE
        DeviceType.TABLET -> RustDeviceType.TABLET
        DeviceType.TV -> RustDeviceType.TV
        DeviceType.VR -> RustDeviceType.VR
        DeviceType.UNKNOWN -> RustDeviceType.UNKNOWN
    }
}

/**
 * FxA and Sync libraries both define a "DeviceType", so we get to have even more cruft.
 */
fun DeviceType.intoSyncType(): mozilla.appservices.syncmanager.DeviceType {
    return when (this) {
        DeviceType.DESKTOP -> mozilla.appservices.syncmanager.DeviceType.DESKTOP
        DeviceType.MOBILE -> mozilla.appservices.syncmanager.DeviceType.MOBILE
        DeviceType.TABLET -> mozilla.appservices.syncmanager.DeviceType.TABLET
        DeviceType.TV -> mozilla.appservices.syncmanager.DeviceType.TV
        DeviceType.VR -> mozilla.appservices.syncmanager.DeviceType.VR
        // There's not a corresponding syncmanager type, so we pick a default for simplicity's sake.
        DeviceType.UNKNOWN -> mozilla.appservices.syncmanager.DeviceType.MOBILE
    }
}

/**
 * Convert between the native-code DeviceCapability data class
 * and the one from the corresponding a-c concept.
 */
fun DeviceCapability.into(): RustDeviceCapability {
    return when (this) {
        DeviceCapability.SEND_TAB -> RustDeviceCapability.SEND_TAB
    }
}

/**
 * Convert between the a-c concept DeviceCapability class and the corresponding
 * native-code DeviceCapability data class.
 */
fun RustDeviceCapability.into(): DeviceCapability {
    return when (this) {
        RustDeviceCapability.SEND_TAB -> DeviceCapability.SEND_TAB
    }
}

/**
 * Convert between the a-c concept DevicePushSubscription class and the corresponding
 * native-code DevicePushSubscription data class.
 */
fun mozilla.components.concept.sync.DevicePushSubscription.into(): RustDevicePushSubscription {
    return RustDevicePushSubscription(
        endpoint = this.endpoint,
        authKey = this.authKey,
        publicKey = this.publicKey,
    )
}

/**
 * Convert between the native-code DevicePushSubscription data class
 * and the one from the corresponding a-c concept.
 */
fun RustDevicePushSubscription.into(): mozilla.components.concept.sync.DevicePushSubscription {
    return mozilla.components.concept.sync.DevicePushSubscription(
        endpoint = this.endpoint,
        authKey = this.authKey,
        publicKey = this.publicKey,
    )
}

fun Device.into(): mozilla.components.concept.sync.Device {
    return mozilla.components.concept.sync.Device(
        id = this.id,
        isCurrentDevice = this.isCurrentDevice,
        deviceType = this.deviceType.into(),
        displayName = this.displayName,
        lastAccessTime = this.lastAccessTime,
        subscriptionExpired = this.pushEndpointExpired,
        capabilities = this.capabilities.map { it.into() },
        subscription = this.pushSubscription?.into(),
    )
}

fun mozilla.components.concept.sync.Device.into(): Device {
    return Device(
        id = this.id,
        isCurrentDevice = this.isCurrentDevice,
        deviceType = this.deviceType.into(),
        displayName = this.displayName,
        lastAccessTime = this.lastAccessTime,
        pushEndpointExpired = this.subscriptionExpired,
        capabilities = this.capabilities.map { it.into() },
        pushSubscription = this.subscription?.into(),
    )
}

fun TabHistoryEntry.into(): mozilla.components.concept.sync.TabData {
    return mozilla.components.concept.sync.TabData(
        title = this.title,
        url = this.url,
    )
}

fun mozilla.components.concept.sync.TabData.into(): TabHistoryEntry {
    return TabHistoryEntry(
        title = this.title,
        url = this.url,
    )
}

fun AccountEvent.into(): mozilla.components.concept.sync.AccountEvent {
    return when (this) {
        is AccountEvent.CommandReceived ->
            mozilla.components.concept.sync.AccountEvent.DeviceCommandIncoming(command = this.command.into())
        is AccountEvent.ProfileUpdated ->
            mozilla.components.concept.sync.AccountEvent.ProfileUpdated
        is AccountEvent.AccountAuthStateChanged ->
            mozilla.components.concept.sync.AccountEvent.AccountAuthStateChanged
        is AccountEvent.AccountDestroyed ->
            mozilla.components.concept.sync.AccountEvent.AccountDestroyed
        is AccountEvent.DeviceConnected ->
            mozilla.components.concept.sync.AccountEvent.DeviceConnected(deviceName = this.deviceName)
        is AccountEvent.DeviceDisconnected ->
            mozilla.components.concept.sync.AccountEvent.DeviceDisconnected(
                deviceId = this.deviceId,
                isLocalDevice = this.isLocalDevice,
            )
    }
}

fun IncomingDeviceCommand.into(): mozilla.components.concept.sync.DeviceCommandIncoming {
    return when (this) {
        is IncomingDeviceCommand.TabReceived -> this.into()
    }
}

fun IncomingDeviceCommand.TabReceived.into(): mozilla.components.concept.sync.DeviceCommandIncoming.TabReceived {
    return mozilla.components.concept.sync.DeviceCommandIncoming.TabReceived(
        from = this.sender?.into(),
        entries = this.payload.entries.map { it.into() },
    )
}

/**
 * Conversion function from fxaclient's data structure to ours.
 */
fun MigrationState.into(): InFlightMigrationState? {
    return when (this) {
        MigrationState.NONE -> null
        MigrationState.COPY_SESSION_TOKEN -> InFlightMigrationState.COPY_SESSION_TOKEN
        MigrationState.REUSE_SESSION_TOKEN -> InFlightMigrationState.REUSE_SESSION_TOKEN
    }
}
