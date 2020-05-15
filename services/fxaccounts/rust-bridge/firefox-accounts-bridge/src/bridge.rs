/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::punt::{
    error::{Error, Result},
    PuntTask,
};
use fxa_client::FirefoxAccount;
use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsCString};
use once_cell::unsync::OnceCell;
use std::{
    str,
    sync::{Arc, Mutex},
};
use storage_variant::HashPropertyBag;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{mozIFirefoxAccountsBridgeCallback, nsIPropertyBag, nsISerialEventTarget},
    RefPtr,
};

/// This macro calls `PuntTask::for_<fn_name>(fxa, <..args>, callback)`
/// if `fxa` has been initialized.
macro_rules! punt {
    ($fn_name:ident $(, $arg:ident)*) => {
        paste::expr! {
            if let Some(fxa) = self.fxa.get() {
                let task = PuntTask::[<for_ $fn_name>](fxa $(, $arg)* ,callback)?;
                task.dispatch(&self.thread)?;
                Ok(())
            } else {
                Err(Error::Unavailable)
            }
        }
    };
}

/// An XPCOM binding for the Rust Firefox Accounts crate.
#[derive(xpcom)]
#[xpimplements(mozIFirefoxAccountsBridge)]
#[refcnt = "nonatomic"] // Non-atomic because we have a `RefCell`.
pub struct InitBridge {
    // A background task queue used to run all our operations
    // on a thread pool.
    thread: RefPtr<nsISerialEventTarget>,
    fxa: OnceCell<Arc<Mutex<FirefoxAccount>>>,
}

impl Bridge {
    pub fn new() -> Result<RefPtr<Bridge>> {
        let thread = moz_task::create_background_task_queue(cstr!("FirefoxAccountsBridge"))?;
        Ok(Bridge::allocate(InitBridge {
            thread,
            fxa: OnceCell::new(),
        }))
    }

    // This method (or InitFromJSON) must be called before any other one, because this
    // is where we can finally give parameters for the Rust `FirefoxAccount` constructor
    // and create an instance.
    xpcom_method!(
        init => Init(
            options: *const nsIPropertyBag
        )
    );
    fn init(&self, options: &nsIPropertyBag) -> Result<()> {
        let options = HashPropertyBag::clone_from_bag(options)?;
        let content_url: nsCString = options.get("content_url")?;
        let content_url = str::from_utf8(&*content_url)?;
        let client_id: nsCString = options.get("client_id")?;
        let client_id = str::from_utf8(&*client_id)?;
        let redirect_uri: nsCString = options.get("redirect_uri")?;
        let redirect_uri = str::from_utf8(&*redirect_uri)?;
        let token_server_url_override: nsCString = options.get("token_server_url_override")?;
        let token_server_url_override = if token_server_url_override.is_empty() {
            None
        } else {
            Some(str::from_utf8(&*token_server_url_override)?)
        };
        self.fxa
            .set(Arc::new(Mutex::new(FirefoxAccount::new(
                &content_url,
                &client_id,
                &redirect_uri,
                token_server_url_override,
            ))))
            .map_err(|_| Error::AlreadyInitialized)?;
        Ok(())
    }

    xpcom_method!(
        from_json => InitFromJSON(
            json: *const nsACString
        )
    );
    fn from_json(&self, json: &nsACString) -> Result<()> {
        let json = str::from_utf8(&*json)?;
        self.fxa
            .set(Arc::new(Mutex::new(FirefoxAccount::from_json(json)?)))
            .map_err(|_| Error::AlreadyInitialized)?;
        Ok(())
    }

    xpcom_method!(
        to_json => StateJSON(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn to_json(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(to_json)
    }

    xpcom_method!(
        begin_oauth_flow => BeginOAuthFlow(
            scopes: *const ThinVec<nsCString>,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn begin_oauth_flow(
        &self,
        scopes: &ThinVec<nsCString>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(begin_oauth_flow, scopes)
    }

    xpcom_method!(
        complete_oauth_flow => CompleteOAuthFlow(
            code: *const nsACString,
            state: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn complete_oauth_flow(
        &self,
        code: &nsACString,
        state: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(complete_oauth_flow, code, state)
    }

    xpcom_method!(
        disconnect => Disconnect(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn disconnect(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(disconnect)
    }

    xpcom_method!(
        get_access_token => GetAccessToken(
            scope: *const nsACString,
            ttl: u64,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_access_token(
        &self,
        scope: &nsACString,
        ttl: u64,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_access_token, scope, ttl)
    }

    xpcom_method!(
        get_session_token => GetSessionToken(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_session_token(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(get_session_token)
    }

    xpcom_method!(
        get_attached_clients => GetAttachedClients(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_attached_clients(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(get_attached_clients)
    }

    xpcom_method!(
        check_authorization_status => CheckAuthorizationStatus(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn check_authorization_status(
        &self,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(check_authorization_status)
    }

    xpcom_method!(
        clear_access_token_cache => ClearAccessTokenCache(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn clear_access_token_cache(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(clear_access_token_cache)
    }

    xpcom_method!(
        handle_session_token_change => HandleSessionTokenChange(
            session_token: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn handle_session_token_change(
        &self,
        session_token: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(handle_session_token_change, session_token)
    }

    xpcom_method!(
        migrate_from_session_token => MigrateFromSessionToken(
            session_token: *const nsACString,
            k_sync: *const nsACString,
            k_xcs: *const nsACString,
            copy_session_token: bool,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn migrate_from_session_token(
        &self,
        session_token: &nsACString,
        k_sync: &nsACString,
        k_xcs: &nsACString,
        copy_session_token: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(
            migrate_from_session_token,
            session_token,
            k_sync,
            k_xcs,
            copy_session_token
        )
    }

    xpcom_method!(
        retry_migrate_from_session_token => RetryMigrateFromSessionToken(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn retry_migrate_from_session_token(
        &self,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(retry_migrate_from_session_token)
    }

    xpcom_method!(
        is_in_migration_state => IsInMigrationState(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn is_in_migration_state(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(is_in_migration_state)
    }

    xpcom_method!(
        get_profile => GetProfile(
            ignore_cache: bool,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_profile(
        &self,
        ignore_cache: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_profile, ignore_cache)
    }

    xpcom_method!(
        get_token_server_endpoint_url => GetTokenServerEndpointURL(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_token_server_endpoint_url(
        &self,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_token_server_endpoint_url)
    }

    xpcom_method!(
        get_connection_success_url => GetConnectionSuccessURL(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_connection_success_url(
        &self,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_connection_success_url)
    }

    xpcom_method!(
        get_manage_account_url => GetManageAccountURL(
            entrypoint: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_manage_account_url(
        &self,
        entrypoint: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_manage_account_url, entrypoint)
    }

    xpcom_method!(
        get_manage_devices_url => GetManageDevicesURL(
            entrypoint: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn get_manage_devices_url(
        &self,
        entrypoint: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(get_manage_devices_url, entrypoint)
    }

    xpcom_method!(
        fetch_devices => FetchDevices(
            ignore_cache: bool,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn fetch_devices(
        &self,
        ignore_cache: bool,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(fetch_devices, ignore_cache)
    }

    xpcom_method!(
        set_device_display_name => SetDeviceDisplayName(
            name: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn set_device_display_name(
        &self,
        name: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(set_device_display_name, name)
    }

    xpcom_method!(
        handle_push_message => HandlePushMessage(
            payload: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn handle_push_message(
        &self,
        payload: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(handle_push_message, payload)
    }

    xpcom_method!(
        poll_device_commands => PollDeviceCommands(
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn poll_device_commands(&self, callback: &mozIFirefoxAccountsBridgeCallback) -> Result<()> {
        punt!(poll_device_commands)
    }

    xpcom_method!(
        send_single_tab => SendSingleTab(
            target_id: *const nsACString,
            title: *const nsACString,
            url: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn send_single_tab(
        &self,
        target_id: &nsACString,
        title: &nsACString,
        url: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(send_single_tab, target_id, title, url)
    }

    xpcom_method!(
        set_device_push_subscription => SetDevicePushSubscription(
            endpoint: *const nsACString,
            public_key: *const nsACString,
            auth_key: *const nsACString,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn set_device_push_subscription(
        &self,
        endpoint: &nsACString,
        public_key: &nsACString,
        auth_key: &nsACString,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(set_device_push_subscription, endpoint, public_key, auth_key)
    }

    xpcom_method!(
        initialize_device => InitializeDevice(
            name: *const nsACString,
            device_type: *const nsACString,
            supported_capabilities: *const ThinVec<nsCString>,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn initialize_device(
        &self,
        name: &nsACString,
        device_type: &nsACString,
        supported_capabilities: &ThinVec<nsCString>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(initialize_device, name, device_type, supported_capabilities)
    }

    xpcom_method!(
        ensure_capabilities => EnsureCapabilities(
            supported_capabilities: *const ThinVec<nsCString>,
            callback: *const mozIFirefoxAccountsBridgeCallback
        )
    );
    fn ensure_capabilities(
        &self,
        supported_capabilities: &ThinVec<nsCString>,
        callback: &mozIFirefoxAccountsBridgeCallback,
    ) -> Result<()> {
        punt!(ensure_capabilities, supported_capabilities)
    }
}
