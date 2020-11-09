/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fxa_client::device::{
    Capability as FxaDeviceCapability, PushSubscription as FxAPushSubscription,
    Type as FxaDeviceType,
};
use nsstring::nsCString;
use storage_variant::VariantType;
use xpcom::{interfaces::nsIVariant, RefPtr};

/// An operation that runs on the background thread, and optionally passes a
/// result to its callback.
pub enum Punt {
    ToJson,
    BeginOAuthFlow(Vec<String>, String),
    CompleteOAuthFlow(String, String),
    Disconnect,
    GetAccessToken(String, Option<u64>),
    GetSessionToken,
    GetAttachedClients,
    CheckAuthorizationStatus,
    ClearAccessTokenCache,
    HandleSessionTokenChange(String),
    MigrateFromSessionToken(String, String, String, bool),
    RetryMigrateFromSessionToken,
    IsInMigrationState,
    GetProfile(bool),
    GetTokenServerEndpointUrl,
    GetConnectionSuccessUrl,
    GetManageAccountUrl(String),
    GetManageDevicesUrl(String),
    FetchDevices(bool),
    SetDeviceDisplayName(String),
    HandlePushMessage(String),
    PollDeviceCommands,
    SendSingleTab(String, String, String),
    SetDevicePushSubscription(FxAPushSubscription),
    InitializeDevice(String, FxaDeviceType, Vec<FxaDeviceCapability>),
    EnsureCapabilities(Vec<FxaDeviceCapability>),
}

impl Punt {
    /// Returns the operation name for debugging and labeling the task
    /// runnable.
    pub fn name(&self) -> &'static str {
        match self {
            Punt::ToJson => concat!(module_path!(), "toJson"),
            Punt::BeginOAuthFlow { .. } => concat!(module_path!(), "beginOAuthFlow"),
            Punt::CompleteOAuthFlow { .. } => concat!(module_path!(), "completeOAuthFlow"),
            Punt::Disconnect => concat!(module_path!(), "disconnect"),
            Punt::GetAccessToken { .. } => concat!(module_path!(), "getAccessToken"),
            Punt::GetSessionToken => concat!(module_path!(), "getSessionToken"),
            Punt::GetAttachedClients => concat!(module_path!(), "getAttachedClients"),
            Punt::CheckAuthorizationStatus => concat!(module_path!(), "checkAuthorizationStatus"),
            Punt::ClearAccessTokenCache => concat!(module_path!(), "clearAccessTokenCache"),
            Punt::HandleSessionTokenChange { .. } => {
                concat!(module_path!(), "handleSessionTokenChange")
            }
            Punt::MigrateFromSessionToken { .. } => {
                concat!(module_path!(), "migrateFromSessionToken")
            }
            Punt::RetryMigrateFromSessionToken => {
                concat!(module_path!(), "retryMigrateFromSessionToken")
            }
            Punt::IsInMigrationState => concat!(module_path!(), "isInMigrationState"),
            Punt::GetProfile { .. } => concat!(module_path!(), "getProfile"),
            Punt::GetTokenServerEndpointUrl => concat!(module_path!(), "getTokenServerEndpointUrl"),
            Punt::GetConnectionSuccessUrl => concat!(module_path!(), "getConnectionSuccessUrl"),
            Punt::GetManageAccountUrl { .. } => concat!(module_path!(), "getManageAccountUrl"),
            Punt::GetManageDevicesUrl { .. } => concat!(module_path!(), "getManageDevicesUrl"),
            Punt::FetchDevices { .. } => concat!(module_path!(), "fetchDevices"),
            Punt::SetDeviceDisplayName { .. } => concat!(module_path!(), "setDeviceDisplayName"),
            Punt::HandlePushMessage { .. } => concat!(module_path!(), "handlePushMessage"),
            Punt::PollDeviceCommands => concat!(module_path!(), "pollDeviceCommands"),
            Punt::SendSingleTab { .. } => concat!(module_path!(), "sendSingleTab"),
            Punt::SetDevicePushSubscription { .. } => {
                concat!(module_path!(), "setDevicePushSubscription")
            }
            Punt::InitializeDevice { .. } => concat!(module_path!(), "initializeDevice"),
            Punt::EnsureCapabilities { .. } => concat!(module_path!(), "ensureCapabilities"),
        }
    }
}

/// The result of a punt task, sent from the background thread back to the
/// main thread. Results are converted to variants, and passed as arguments to
/// the callbacks.
pub enum PuntResult {
    String(String),
    Boolean(bool),
    Null,
}

impl PuntResult {
    pub fn url_spec(url: url::Url) -> Self {
        PuntResult::String(url.into_string())
    }
    pub fn json_stringify<T>(value: T) -> Self
    where
        T: serde::Serialize + std::marker::Sized,
    {
        PuntResult::String(serde_json::to_string(&value).unwrap())
    }
}

impl From<()> for PuntResult {
    fn from(_: ()) -> PuntResult {
        PuntResult::Null
    }
}

impl PuntResult {
    /// Converts the result to an `nsIVariant` that can be passed as an
    /// argument to `callback.handleResult()`.
    pub fn into_variant(self) -> RefPtr<nsIVariant> {
        match self {
            PuntResult::String(v) => nsCString::from(v).into_variant(),
            PuntResult::Boolean(b) => b.into_variant(),
            PuntResult::Null => ().into_variant(),
        }
    }
}
