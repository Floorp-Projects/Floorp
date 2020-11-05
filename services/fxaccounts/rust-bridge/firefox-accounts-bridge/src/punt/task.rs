/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::punt::{
    error::{self, Error},
    punt::{Punt, PuntResult},
};
use atomic_refcell::AtomicRefCell;
use fxa_client::{
    device::{
        Capability as FxaDeviceCapability,
        CommandFetchReason,
        PushSubscription as FxaPushSubscription,
        Type as FxaDeviceType,
    },
    FirefoxAccount,
};
use moz_task::{DispatchOptions, Task, TaskRunnable, ThreadPtrHandle, ThreadPtrHolder};
use nserror::nsresult;
use nsstring::{nsACString, nsCString};
use std::{
    fmt::Write,
    mem, str,
    sync::{Arc, Mutex, Weak},
};
use xpcom::{
    interfaces::{mozIFirefoxAccountsBridgeCallback, nsIEventTarget},
    RefPtr,
};

/// A punt task sends an operation to an fxa instance on a
/// background thread or task queue, and ferries back an optional result to
/// a callback.
pub struct PuntTask {
    name: &'static str,
    fxa: Weak<Mutex<FirefoxAccount>>,
    punt: AtomicRefCell<Option<Punt>>,
    callback: ThreadPtrHandle<mozIFirefoxAccountsBridgeCallback>,
    result: AtomicRefCell<Result<PuntResult, Error>>,
}

impl PuntTask {
    /// Creates a task for a punt. The `callback` is bound to the current
    /// thread, and will be called once, after the punt returns from the
    /// background thread.
    fn new(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        punt: Punt,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let name = punt.name();
        Ok(Self {
            name: punt.name(),
            fxa: Arc::downgrade(fxa),
            punt: AtomicRefCell::new(Some(punt)),
            callback: ThreadPtrHolder::new(
                cstr!("mozIFirefoxAccountsBridgeCallback"),
                RefPtr::new(callback),
            )?,
            result: AtomicRefCell::new(Err(Error::DidNotRun(name).into())),
        })
    }

    /// Creates a task that calls begin_oauth_flow.
    #[inline]
    pub fn for_begin_oauth_flow(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        scopes: &[nsCString],
        entry_point: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let scopes = scopes.iter().try_fold(
            Vec::with_capacity(scopes.len()),
            |mut acc, scope| -> error::Result<_> {
                acc.push(std::str::from_utf8(&*scope)?.into());
                Ok(acc)
            },
        )?;
        let entry_point = str::from_utf8(&*entry_point)?.into();
        Self::new(fxa, Punt::BeginOAuthFlow(scopes, entry_point), callback)
    }

    /// Creates a task that calls complete_oauth_flow.
    #[inline]
    pub fn for_complete_oauth_flow(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        code: &nsACString,
        state: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let code = str::from_utf8(&*code)?.into();
        let state = str::from_utf8(&*state)?.into();
        Self::new(fxa, Punt::CompleteOAuthFlow(code, state), callback)
    }

    /// Creates a task that calls disconnect.
    #[inline]
    pub fn for_disconnect(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::Disconnect, callback)
    }

    /// Creates a task that calls to_json.
    #[inline]
    pub fn for_to_json(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::ToJson, callback)
    }

    /// Creates a task that calls get_access_token.
    #[inline]
    pub fn for_get_access_token(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        scope: &nsACString,
        ttl: u64,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let scope = str::from_utf8(&*scope)?.into();
        let ttl = if ttl > 0 { Some(ttl) } else { None };
        Self::new(fxa, Punt::GetAccessToken(scope, ttl), callback)
    }

    /// Creates a task that calls get_session_token.
    #[inline]
    pub fn for_get_session_token(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::GetSessionToken, callback)
    }

    /// Creates a task that calls get_attached_clients.
    #[inline]
    pub fn for_get_attached_clients(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::GetAttachedClients, callback)
    }

    /// Creates a task that calls check_authorization_status.
    #[inline]
    pub fn for_check_authorization_status(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::CheckAuthorizationStatus, callback)
    }

    /// Creates a task that calls clear_access_token_cache.
    #[inline]
    pub fn for_clear_access_token_cache(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::ClearAccessTokenCache, callback)
    }

    /// Creates a task that calls handle_session_token_change.
    #[inline]
    pub fn for_handle_session_token_change(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        session_token: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let session_token = str::from_utf8(&*session_token)?.into();
        Self::new(fxa, Punt::HandleSessionTokenChange(session_token), callback)
    }

    /// Creates a task that calls migrate_from_session_token.
    #[inline]
    pub fn for_migrate_from_session_token(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        session_token: &nsACString,
        k_sync: &nsACString,
        k_xcs: &nsACString,
        copy_session_token: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let session_token = str::from_utf8(&*session_token)?.into();
        let k_sync = str::from_utf8(&*k_sync)?.into();
        let k_xcs = str::from_utf8(&*k_xcs)?.into();
        Self::new(
            fxa,
            Punt::MigrateFromSessionToken(session_token, k_sync, k_xcs, copy_session_token),
            callback,
        )
    }

    /// Creates a task that calls retry_migrate_from_session_token.
    #[inline]
    pub fn for_retry_migrate_from_session_token(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::RetryMigrateFromSessionToken, callback)
    }

    /// Creates a task that calls is_in_migration_state.
    #[inline]
    pub fn for_is_in_migration_state(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::IsInMigrationState, callback)
    }

    /// Creates a task that calls get_profile.
    #[inline]
    pub fn for_get_profile(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        ignore_cache: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::GetProfile(ignore_cache), callback)
    }

    /// Creates a task that calls get_token_server_endpoint_url.
    #[inline]
    pub fn for_get_token_server_endpoint_url(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::GetTokenServerEndpointUrl, callback)
    }

    /// Creates a task that calls get_connection_success_url.
    #[inline]
    pub fn for_get_connection_success_url(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::GetConnectionSuccessUrl, callback)
    }

    /// Creates a task that calls get_manage_account_url.
    #[inline]
    pub fn for_get_manage_account_url(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        entrypoint: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let entrypoint = str::from_utf8(&*entrypoint)?.into();
        Self::new(fxa, Punt::GetManageAccountUrl(entrypoint), callback)
    }

    /// Creates a task that calls get_manage_devices_url.
    #[inline]
    pub fn for_get_manage_devices_url(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        entrypoint: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let entrypoint = str::from_utf8(&*entrypoint)?.into();
        Self::new(fxa, Punt::GetManageDevicesUrl(entrypoint), callback)
    }

    /// Creates a task that calls fetch_devices.
    #[inline]
    pub fn for_fetch_devices(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        ignore_cache: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::FetchDevices(ignore_cache), callback)
    }

    /// Creates a task that calls set_device_display_name.
    #[inline]
    pub fn for_set_device_display_name(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        name: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let name = str::from_utf8(&*name)?.into();
        Self::new(fxa, Punt::SetDeviceDisplayName(name), callback)
    }

    /// Creates a task that calls handle_push_message.
    #[inline]
    pub fn for_handle_push_message(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        payload: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let payload = str::from_utf8(&*payload)?.into();
        Self::new(fxa, Punt::HandlePushMessage(payload), callback)
    }

    /// Creates a task that calls poll_device_commands.
    #[inline]
    pub fn for_poll_device_commands(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        Self::new(fxa, Punt::PollDeviceCommands, callback)
    }

    /// Creates a task that calls send_single_tab.
    #[inline]
    pub fn for_send_single_tab(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        target_id: &nsACString,
        title: &nsACString,
        url: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let target_id = str::from_utf8(&*target_id)?.into();
        let title = str::from_utf8(&*title)?.into();
        let url = str::from_utf8(&*url)?.into();
        Self::new(fxa, Punt::SendSingleTab(target_id, title, url), callback)
    }

    /// Creates a task that calls set_device_push_subscription.
    #[inline]
    pub fn for_set_device_push_subscription(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        endpoint: &nsACString,
        public_key: &nsACString,
        auth_key: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let push_subscription = FxaPushSubscription {
            endpoint: str::from_utf8(&*endpoint)?.into(),
            public_key: str::from_utf8(&*public_key)?.into(),
            auth_key: str::from_utf8(&*auth_key)?.into(),
        };
        Self::new(
            fxa,
            Punt::SetDevicePushSubscription(push_subscription),
            callback,
        )
    }

    /// Creates a task that calls initialize_device.
    #[inline]
    pub fn for_initialize_device(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        name: &nsACString,
        device_type: &nsACString,
        supported_capabilities: &[nsCString],
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let name = str::from_utf8(&*name)?.into();
        let device_type = to_device_type(device_type)?;
        let supported_capabilities = to_capabilities(supported_capabilities)?;
        Self::new(
            fxa,
            Punt::InitializeDevice(name, device_type, supported_capabilities),
            callback,
        )
    }

    /// Creates a task that calls ensure_capabilities.
    #[inline]
    pub fn for_ensure_capabilities(
        fxa: &Arc<Mutex<FirefoxAccount>>,
        supported_capabilities: &[nsCString],
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> error::Result<PuntTask> {
        let supported_capabilities = to_capabilities(supported_capabilities)?;
        Self::new(
            fxa,
            Punt::EnsureCapabilities(supported_capabilities),
            callback,
        )
    }

    /// Dispatches the task to the given thread `target`.
    pub fn dispatch(self, target: &nsIEventTarget) -> Result<(), Error> {
        let runnable = TaskRunnable::new(self.name, Box::new(self))?;
        // `may_block` schedules the task on the I/O thread pool, since we
        // expect most operations to wait on I/O.
        TaskRunnable::dispatch_with_options(
            runnable,
            target,
            DispatchOptions::default().may_block(true),
        )?;
        Ok(())
    }

    fn run_with_punt(&self, punt: Punt) -> Result<PuntResult, Error> {
        let fxa = self.fxa.upgrade().ok_or_else(|| Error::AlreadyTornDown)?;
        let mut fxa = fxa.lock()?;
        Ok(match punt {
            Punt::ToJson => fxa.to_json().map(PuntResult::String),
            Punt::BeginOAuthFlow(scopes, entry_point) => {
                let scopes: Vec<&str> = scopes.iter().map(AsRef::as_ref).collect();
                fxa.begin_oauth_flow(&scopes, &entry_point, None).map(PuntResult::String)
            }
            Punt::CompleteOAuthFlow(code, state) => fxa
                .complete_oauth_flow(&code, &state)
                .map(|_| PuntResult::Null),
            Punt::Disconnect => {
                fxa.disconnect();
                Ok(PuntResult::Null)
            }
            Punt::GetAccessToken(scope, ttl) => fxa
                .get_access_token(&scope, ttl)
                .map(PuntResult::json_stringify),
            Punt::GetSessionToken => fxa.get_session_token().map(PuntResult::String),
            Punt::GetAttachedClients => fxa.get_attached_clients().map(PuntResult::json_stringify),
            Punt::CheckAuthorizationStatus => fxa
                .check_authorization_status()
                .map(PuntResult::json_stringify),
            Punt::ClearAccessTokenCache => {
                fxa.clear_access_token_cache();
                Ok(PuntResult::Null)
            }
            Punt::HandleSessionTokenChange(session_token) => fxa
                .handle_session_token_change(&session_token)
                .map(|_| PuntResult::Null),
            Punt::MigrateFromSessionToken(session_token, k_sync, k_xcs, copy_session_token) => fxa
                .migrate_from_session_token(&session_token, &k_sync, &k_xcs, copy_session_token)
                .map(PuntResult::json_stringify),
            Punt::RetryMigrateFromSessionToken => {
                fxa.try_migration().map(PuntResult::json_stringify)
            }
            Punt::IsInMigrationState => {
                Ok(PuntResult::Boolean(match fxa.is_in_migration_state() {
                    fxa_client::migrator::MigrationState::None => false,
                    _ => true,
                }))
            }
            Punt::GetProfile(ignore_cache) => fxa
                .get_profile(ignore_cache)
                .map(PuntResult::json_stringify),
            Punt::GetTokenServerEndpointUrl => fxa
                .get_token_server_endpoint_url()
                .map(PuntResult::url_spec),
            Punt::GetConnectionSuccessUrl => {
                fxa.get_connection_success_url().map(PuntResult::url_spec)
            }
            Punt::GetManageAccountUrl(entrypoint) => fxa
                .get_manage_account_url(&entrypoint)
                .map(PuntResult::url_spec),
            Punt::GetManageDevicesUrl(entrypoint) => fxa
                .get_manage_devices_url(&entrypoint)
                .map(PuntResult::url_spec),
            Punt::FetchDevices(ignore_cache) => fxa
                .get_devices(ignore_cache)
                .map(PuntResult::json_stringify),
            Punt::SetDeviceDisplayName(name) => {
                fxa.set_device_name(&name).map(PuntResult::json_stringify)
            }
            Punt::HandlePushMessage(payload) => fxa
                .handle_push_message(&payload)
                .map(PuntResult::json_stringify),
            Punt::PollDeviceCommands => fxa.poll_device_commands(CommandFetchReason::Poll).map(PuntResult::json_stringify),
            Punt::SendSingleTab(target_id, title, url) => fxa
                .send_tab(&target_id, &title, &url)
                .map(|_| PuntResult::Null),
            Punt::SetDevicePushSubscription(push_subscription) => fxa
                .set_push_subscription(&push_subscription)
                .map(PuntResult::json_stringify),
            Punt::InitializeDevice(name, device_type, capabilities) => fxa
                .initialize_device(&name, device_type.clone(), &capabilities)
                .map(|_| PuntResult::Null),
            Punt::EnsureCapabilities(capabilities) => fxa
                .ensure_capabilities(&capabilities)
                .map(|_| PuntResult::Null),
        }?)
    }
}

fn to_capabilities(capabilities: &[nsCString]) -> error::Result<Vec<FxaDeviceCapability>> {
    Ok(capabilities.iter().try_fold(
        Vec::with_capacity(capabilities.len()),
        |mut acc, capability| -> error::Result<_> {
            let capability = str::from_utf8(&*capability)?;
            acc.push(match capability {
                "sendTab" => FxaDeviceCapability::SendTab,
                _ => return Err(Error::Nsresult(nserror::NS_ERROR_INVALID_ARG)),
            });
            Ok(acc)
        },
    )?)
}

fn to_device_type(device_type: &nsACString) -> error::Result<FxaDeviceType> {
    let device_type = str::from_utf8(&*device_type)?;
    Ok(match device_type {
        "desktop" => FxaDeviceType::Desktop,
        "mobile" => FxaDeviceType::Mobile,
        "tablet" => FxaDeviceType::Tablet,
        "tv" => FxaDeviceType::TV,
        "vr" => FxaDeviceType::VR,
        _ => return Err(Error::Nsresult(nserror::NS_ERROR_INVALID_ARG)),
    })
}

impl Task for PuntTask {
    fn run(&self) {
        *self.result.borrow_mut() = match mem::take(&mut *self.punt.borrow_mut()) {
            Some(punt) => self.run_with_punt(punt),
            // A task should never run on the background queue twice, but we
            // return an error just in case.
            None => Err(Error::AlreadyRan(self.name)),
        };
    }

    fn done(&self) -> Result<(), nsresult> {
        let callback = self.callback.get().unwrap();
        match mem::replace(
            &mut *self.result.borrow_mut(),
            Err(Error::AlreadyRan(self.name)),
        ) {
            Ok(result) => unsafe { callback.HandleSuccess(result.into_variant().coerce()) },
            Err(err) => {
                let mut message = nsCString::new();
                write!(message, "{}", err).unwrap();
                unsafe { callback.HandleError(err.into(), &*message) }
            }
        }
        .to_result()
    }
}
