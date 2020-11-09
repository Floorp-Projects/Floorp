/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::http_client::{
    DeviceLocation as Location, DeviceType as Type, GetDeviceResponse as Device, PushSubscription,
};
use crate::{
    commands,
    error::*,
    http_client::{
        DeviceUpdateRequest, DeviceUpdateRequestBuilder, PendingCommand, UpdateDeviceResponse,
    },
    telemetry, util, CachedResponse, FirefoxAccount, IncomingDeviceCommand,
};
use serde_derive::*;
use std::collections::{HashMap, HashSet};

// An devices response is considered fresh for `DEVICES_FRESHNESS_THRESHOLD` ms.
const DEVICES_FRESHNESS_THRESHOLD: u64 = 60_000; // 1 minute

/// The reason we are fetching commands.
#[derive(Clone, Copy)]
pub enum CommandFetchReason {
    /// We are polling in-case we've missed some.
    Poll,
    /// We got a push notification with the index of the message.
    Push(u64),
}

impl FirefoxAccount {
    /// Fetches the list of devices from the current account including
    /// the current one.
    ///
    /// * `ignore_cache` - If set to true, bypass the in-memory cache
    /// and fetch devices from the server.
    pub fn get_devices(&mut self, ignore_cache: bool) -> Result<Vec<Device>> {
        if let Some(d) = &self.devices_cache {
            if !ignore_cache && util::now() < d.cached_at + DEVICES_FRESHNESS_THRESHOLD {
                return Ok(d.response.clone());
            }
        }

        let refresh_token = self.get_refresh_token()?;
        let response = self.client.devices(&self.state.config, &refresh_token)?;

        self.devices_cache = Some(CachedResponse {
            response: response.clone(),
            cached_at: util::now(),
            etag: "".into(),
        });

        Ok(response)
    }

    pub fn get_current_device(&mut self) -> Result<Option<Device>> {
        Ok(self
            .get_devices(false)?
            .into_iter()
            .find(|d| d.is_current_device))
    }

    /// Replaces the internal set of "tracked" device capabilities by re-registering
    /// new capabilities and returns a set of device commands to register with the
    /// server.
    fn register_capabilities(
        &mut self,
        capabilities: &[Capability],
    ) -> Result<HashMap<String, String>> {
        let mut capabilities_set = HashSet::new();
        let mut commands = HashMap::new();
        for capability in capabilities {
            match capability {
                Capability::SendTab => {
                    let send_tab_command = self.generate_send_tab_command_data()?;
                    commands.insert(
                        commands::send_tab::COMMAND_NAME.to_owned(),
                        send_tab_command.to_owned(),
                    );
                    capabilities_set.insert(Capability::SendTab);
                }
            }
        }
        // Remember what capabilities we've registered, so we don't register the same ones again.
        // We write this to internal state before we've actually written the new device record,
        // but roll it back if the server update fails.
        self.state.device_capabilities = capabilities_set;
        Ok(commands)
    }

    /// Initalizes our own device, most of the time this will be called right after logging-in
    /// for the first time.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn initialize_device(
        &mut self,
        name: &str,
        device_type: Type,
        capabilities: &[Capability],
    ) -> Result<()> {
        let commands = self.register_capabilities(capabilities)?;
        let update = DeviceUpdateRequestBuilder::new()
            .display_name(name)
            .device_type(&device_type)
            .available_commands(&commands)
            .build();
        let resp = self.update_device(update)?;
        self.state.current_device_id = Option::from(resp.id);
        Ok(())
    }

    /// Register a set of device capabilities against the current device.
    ///
    /// As the only capability is Send Tab now, its command is registered with the server.
    /// Don't forget to also call this if the Sync Keys change as they
    /// encrypt the Send Tab command data.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn ensure_capabilities(&mut self, capabilities: &[Capability]) -> Result<()> {
        // Don't re-register if we already have exactly those capabilities.
        // Because of the way that our state object defaults `device_capabilities` to empty,
        // we can't tell the difference between "have never registered capabilities" and
        // have "explicitly registered an empty set of capabilities", so it's simpler to
        // just always re-register in that case.
        if !self.state.device_capabilities.is_empty()
            && self.state.device_capabilities.len() == capabilities.len()
            && capabilities
                .iter()
                .all(|c| self.state.device_capabilities.contains(c))
        {
            return Ok(());
        }
        let commands = self.register_capabilities(capabilities)?;
        let update = DeviceUpdateRequestBuilder::new()
            .available_commands(&commands)
            .build();
        let resp = self.update_device(update)?;
        self.state.current_device_id = Option::from(resp.id);
        Ok(())
    }

    /// Re-register the device capabilities, this should only be used internally.
    pub(crate) fn reregister_current_capabilities(&mut self) -> Result<()> {
        let current_capabilities: Vec<Capability> =
            self.state.device_capabilities.clone().into_iter().collect();
        let commands = self.register_capabilities(&current_capabilities)?;
        let update = DeviceUpdateRequestBuilder::new()
            .available_commands(&commands)
            .build();
        self.update_device(update)?;
        Ok(())
    }

    pub(crate) fn invoke_command(
        &self,
        command: &str,
        target: &Device,
        payload: &serde_json::Value,
    ) -> Result<()> {
        let refresh_token = self.get_refresh_token()?;
        self.client.invoke_command(
            &self.state.config,
            &refresh_token,
            command,
            &target.id,
            payload,
        )
    }

    /// Poll and parse any pending available command for our device.
    /// This should be called semi-regularly as the main method of
    /// commands delivery (push) can sometimes be unreliable on mobile devices.
    /// Typically called even when a push notification is received, so that
    /// any prior messages for which a push didn't arrive are still handled.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn poll_device_commands(
        &mut self,
        reason: CommandFetchReason,
    ) -> Result<Vec<IncomingDeviceCommand>> {
        let last_command_index = self.state.last_handled_command.unwrap_or(0);
        // We increment last_command_index by 1 because the server response includes the current index.
        self.fetch_and_parse_commands(last_command_index + 1, None, reason)
    }

    /// Retrieve and parse a specific command designated by its index.
    ///
    /// **💾 This method alters the persisted account state.**
    ///
    /// Note that this should not be used if possible, as it does not correctly
    /// handle missed messages. It's currently used only on iOS due to platform
    /// restrictions (but we should still try and work out how to correctly
    /// handle missed messages within those restrictions)
    /// (What's wrong: if we get a push for tab-1 and a push for tab-3, and
    /// between them I've never explicitly polled, I'll miss tab-2, even if I
    /// try polling now)
    pub fn ios_fetch_device_command(&mut self, index: u64) -> Result<IncomingDeviceCommand> {
        let mut device_commands =
            self.fetch_and_parse_commands(index, Some(1), CommandFetchReason::Push(index))?;
        let device_command = device_commands
            .pop()
            .ok_or_else(|| ErrorKind::IllegalState("Index fetch came out empty."))?;
        if !device_commands.is_empty() {
            log::warn!("Index fetch resulted in more than 1 element");
        }
        Ok(device_command)
    }

    fn fetch_and_parse_commands(
        &mut self,
        index: u64,
        limit: Option<u64>,
        reason: CommandFetchReason,
    ) -> Result<Vec<IncomingDeviceCommand>> {
        let refresh_token = self.get_refresh_token()?;
        let pending_commands =
            self.client
                .pending_commands(&self.state.config, refresh_token, index, limit)?;
        if pending_commands.messages.is_empty() {
            return Ok(Vec::new());
        }
        log::info!("Handling {} messages", pending_commands.messages.len());
        let device_commands = self.parse_commands_messages(pending_commands.messages, reason)?;
        self.state.last_handled_command = Some(pending_commands.index);
        Ok(device_commands)
    }

    fn parse_commands_messages(
        &mut self,
        messages: Vec<PendingCommand>,
        reason: CommandFetchReason,
    ) -> Result<Vec<IncomingDeviceCommand>> {
        let devices = self.get_devices(false)?;
        let parsed_commands = messages
            .into_iter()
            .filter_map(|msg| match self.parse_command(msg, &devices, reason) {
                Ok(device_command) => Some(device_command),
                Err(e) => {
                    log::error!("Error while processing command: {}", e);
                    None
                }
            })
            .collect();
        Ok(parsed_commands)
    }

    fn parse_command(
        &mut self,
        command: PendingCommand,
        devices: &[Device],
        reason: CommandFetchReason,
    ) -> Result<IncomingDeviceCommand> {
        let telem_reason = match reason {
            CommandFetchReason::Poll => telemetry::ReceivedReason::Poll,
            CommandFetchReason::Push(index) if command.index < index => {
                telemetry::ReceivedReason::PushMissed
            }
            _ => telemetry::ReceivedReason::Push,
        };
        let command_data = command.data;
        let sender = command_data
            .sender
            .and_then(|s| devices.iter().find(|i| i.id == s).cloned());
        match command_data.command.as_str() {
            commands::send_tab::COMMAND_NAME => {
                self.handle_send_tab_command(sender, command_data.payload, telem_reason)
            }
            _ => Err(ErrorKind::UnknownCommand(command_data.command).into()),
        }
    }

    pub fn set_device_name(&mut self, name: &str) -> Result<UpdateDeviceResponse> {
        let update = DeviceUpdateRequestBuilder::new().display_name(name).build();
        self.update_device(update)
    }

    pub fn clear_device_name(&mut self) -> Result<UpdateDeviceResponse> {
        let update = DeviceUpdateRequestBuilder::new()
            .clear_display_name()
            .build();
        self.update_device(update)
    }

    pub fn set_push_subscription(
        &mut self,
        push_subscription: &PushSubscription,
    ) -> Result<UpdateDeviceResponse> {
        let update = DeviceUpdateRequestBuilder::new()
            .push_subscription(&push_subscription)
            .build();
        self.update_device(update)
    }

    // TODO: this currently overwrites every other registered command
    // for the device because the server does not have a `PATCH commands`
    // endpoint yet.
    #[allow(dead_code)]
    pub(crate) fn register_command(
        &mut self,
        command: &str,
        value: &str,
    ) -> Result<UpdateDeviceResponse> {
        self.state.device_capabilities.clear();
        let mut commands = HashMap::new();
        commands.insert(command.to_owned(), value.to_owned());
        let update = DeviceUpdateRequestBuilder::new()
            .available_commands(&commands)
            .build();
        self.update_device(update)
    }

    // TODO: this currently deletes every command registered for the device
    // because the server does not have a `PATCH commands` endpoint yet.
    #[allow(dead_code)]
    pub(crate) fn unregister_command(&mut self, _: &str) -> Result<UpdateDeviceResponse> {
        self.state.device_capabilities.clear();
        let commands = HashMap::new();
        let update = DeviceUpdateRequestBuilder::new()
            .available_commands(&commands)
            .build();
        self.update_device(update)
    }

    #[allow(dead_code)]
    pub(crate) fn clear_commands(&mut self) -> Result<UpdateDeviceResponse> {
        self.state.device_capabilities.clear();
        let update = DeviceUpdateRequestBuilder::new()
            .clear_available_commands()
            .build();
        self.update_device(update)
    }

    pub(crate) fn replace_device(
        &mut self,
        display_name: &str,
        device_type: &Type,
        push_subscription: &Option<PushSubscription>,
        commands: &HashMap<String, String>,
    ) -> Result<UpdateDeviceResponse> {
        self.state.device_capabilities.clear();
        let mut builder = DeviceUpdateRequestBuilder::new()
            .display_name(display_name)
            .device_type(device_type)
            .available_commands(commands);
        if let Some(push_subscription) = push_subscription {
            builder = builder.push_subscription(push_subscription)
        }
        self.update_device(builder.build())
    }

    fn update_device(&mut self, update: DeviceUpdateRequest<'_>) -> Result<UpdateDeviceResponse> {
        let refresh_token = self.get_refresh_token()?;
        let res = self
            .client
            .update_device(&self.state.config, refresh_token, update);
        match res {
            Ok(resp) => Ok(resp),
            Err(err) => {
                // We failed to write an update to the server.
                // Clear local state so that we'll be sure to retry later.
                self.state.device_capabilities.clear();
                Err(err)
            }
        }
    }

    /// Retrieve the current device id from state
    pub fn get_current_device_id(&mut self) -> Result<String> {
        match self.state.current_device_id {
            Some(ref device_id) => Ok(device_id.to_string()),
            None => Err(ErrorKind::NoCurrentDeviceId.into()),
        }
    }
}

#[derive(Clone, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub enum Capability {
    SendTab,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::http_client::*;
    use crate::oauth::RefreshToken;
    use crate::scoped_keys::ScopedKey;
    use crate::Config;
    use std::collections::HashSet;
    use std::sync::Arc;

    fn setup() -> FirefoxAccount {
        // I'd love to be able to configure a single mocked client here,
        // but can't work out how to do that within the typesystem.
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.state.refresh_token = Some(RefreshToken {
            token: "refreshtok".to_string(),
            scopes: HashSet::default(),
        });
        fxa.state.scoped_keys.insert("https://identity.mozilla.com/apps/oldsync".to_string(), ScopedKey {
            kty: "oct".to_string(),
            scope: "https://identity.mozilla.com/apps/oldsync".to_string(),
            k: "kMtwpVC0ZaYFJymPza8rXK_0CgCp3KMwRStwGfBRBDtL6hXRDVJgQFaoOQ2dimw0Bko5WVv2gNTy7RX5zFYZHg".to_string(),
            kid: "1542236016429-Ox1FbJfFfwTe5t-xq4v2hQ".to_string(),
        });
        fxa
    }

    #[test]
    fn test_ensure_capabilities_does_not_hit_the_server_if_nothing_has_changed() {
        let mut fxa = setup();

        // Do an initial call to ensure_capabilities().
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));
        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();
        let saved = fxa.to_json().unwrap();

        // Do another call with the same capabilities.
        // The FxAClientMock will panic if it tries to hit the network again, which it shouldn't.
        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();

        // Do another call with the same capabilities , after restoring from disk.
        // The FxAClientMock will panic if it tries to hit the network, which it shouldn't.
        let mut restored = FirefoxAccount::from_json(&saved).unwrap();
        restored.set_client(Arc::new(FxAClientMock::new()));
        restored
            .ensure_capabilities(&[Capability::SendTab])
            .unwrap();
    }

    #[test]
    fn test_ensure_capabilities_updates_the_server_if_capabilities_increase() {
        let mut fxa = setup();

        // Do an initial call to ensure_capabilities().
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[]).unwrap();
        let saved = fxa.to_json().unwrap();

        // Do another call with reduced capabilities.
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();

        // Do another call with the same capabilities , after restoring from disk.
        // The FxAClientMock will panic if it tries to hit the network, which it shouldn't.
        let mut restored = FirefoxAccount::from_json(&saved).unwrap();
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        restored.set_client(Arc::new(client));

        restored
            .ensure_capabilities(&[Capability::SendTab])
            .unwrap();
    }

    #[test]
    fn test_ensure_capabilities_updates_the_server_if_capabilities_reduce() {
        let mut fxa = setup();

        // Do an initial call to ensure_capabilities().
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();
        let saved = fxa.to_json().unwrap();

        // Do another call with reduced capabilities.
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[]).unwrap();

        // Do another call with the same capabilities , after restoring from disk.
        // The FxAClientMock will panic if it tries to hit the network, which it shouldn't.
        let mut restored = FirefoxAccount::from_json(&saved).unwrap();
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        restored.set_client(Arc::new(client));

        restored.ensure_capabilities(&[]).unwrap();
    }

    #[test]
    fn test_ensure_capabilities_will_reregister_after_new_login_flow() {
        let mut fxa = setup();

        // Do an initial call to ensure_capabilities().
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));
        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();

        // Fake that we've completed a new login flow.
        // (which annoyingly makes a bunch of network requests)
        let mut client = FxAClientMock::new();
        client
            .expect_destroy_access_token(mockiato::Argument::any, mockiato::Argument::any)
            .returns_once(Err(ErrorKind::RemoteError {
                code: 500,
                errno: 999,
                error: "server error".to_string(),
                message: "this will be ignored anyway".to_string(),
                info: "".to_string(),
            }
            .into()));
        client
            .expect_devices(mockiato::Argument::any, mockiato::Argument::any)
            .returns_once(Err(ErrorKind::RemoteError {
                code: 500,
                errno: 999,
                error: "server error".to_string(),
                message: "this will be ignored anyway".to_string(),
                info: "".to_string(),
            }
            .into()));
        client
            .expect_destroy_refresh_token(mockiato::Argument::any, mockiato::Argument::any)
            .returns_once(Err(ErrorKind::RemoteError {
                code: 500,
                errno: 999,
                error: "server error".to_string(),
                message: "this will be ignored anyway".to_string(),
                info: "".to_string(),
            }
            .into()));
        fxa.set_client(Arc::new(client));

        fxa.handle_oauth_response(
            OAuthTokenResponse {
                keys_jwe: None,
                refresh_token: Some("newRefreshTok".to_string()),
                session_token: None,
                expires_in: 12345,
                scope: "profile".to_string(),
                access_token: "accesstok".to_string(),
            },
            None,
        )
        .unwrap();

        assert!(fxa.state.device_capabilities.is_empty());

        // Do another call with the same capabilities.
        // It should re-register, as server-side state may have changed.
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("newRefreshTok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));
        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();
    }

    #[test]
    fn test_ensure_capabilities_updates_the_server_if_previous_attempt_failed() {
        let mut fxa = setup();

        // Do an initial call to ensure_capabilities(), that fails.
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Err(ErrorKind::RemoteError {
                code: 500,
                errno: 999,
                error: "server error".to_string(),
                message: "this will be ignored anyway".to_string(),
                info: "".to_string(),
            }
            .into()));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap_err();

        // Do another call, which should re-attempt the update.
        let mut client = FxAClientMock::new();
        client
            .expect_update_device(
                mockiato::Argument::any,
                |arg| arg.partial_eq("refreshtok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(UpdateDeviceResponse {
                id: "device1".to_string(),
                display_name: "".to_string(),
                device_type: DeviceType::Desktop,
                push_subscription: None,
                available_commands: HashMap::default(),
                push_endpoint_expired: false,
            }));
        fxa.set_client(Arc::new(client));

        fxa.ensure_capabilities(&[Capability::SendTab]).unwrap();
    }

    #[test]
    fn test_get_devices() {
        let mut fxa = setup();
        let mut client = FxAClientMock::new();
        client
            .expect_devices(mockiato::Argument::any, mockiato::Argument::any)
            .times(1)
            .returns_once(Ok(vec![Device {
                common: DeviceResponseCommon {
                    id: "device1".into(),
                    display_name: "".to_string(),
                    device_type: DeviceType::Desktop,
                    push_subscription: None,
                    available_commands: HashMap::new(),
                    push_endpoint_expired: true,
                },
                is_current_device: true,
                location: DeviceLocation {
                    city: None,
                    country: None,
                    state: None,
                    state_code: None,
                },
                last_access_time: None,
            }]));

        fxa.set_client(Arc::new(client));
        assert!(fxa.devices_cache.is_none());

        assert!(fxa.get_devices(false).is_ok());
        assert!(fxa.devices_cache.is_some());

        let cache = fxa.devices_cache.clone().unwrap();
        assert!(!cache.response.is_empty());
        assert!(cache.cached_at > 0);

        let cached_devices = cache.response;
        assert_eq!(cached_devices[0].id, "device1".to_string());

        // Check that a second call to get_devices doesn't hit the server
        assert!(fxa.get_devices(false).is_ok());
        assert!(fxa.devices_cache.is_some());

        let cache2 = fxa.devices_cache.unwrap();
        let cached_devices2 = cache2.response;

        assert_eq!(cache.cached_at, cache2.cached_at);
        assert_eq!(cached_devices.len(), cached_devices2.len());
        assert_eq!(cached_devices[0].id, cached_devices2[0].id);
    }

    #[test]
    fn test_get_devices_network_errors() {
        let mut fxa = setup();
        let mut client = FxAClientMock::new();
        client
            .expect_devices(mockiato::Argument::any, mockiato::Argument::any)
            .times(1)
            .returns_once(Err(ErrorKind::RemoteError {
                code: 500,
                errno: 101,
                error: "Did not work!".to_owned(),
                message: "Did not work!".to_owned(),
                info: "Did not work!".to_owned(),
            }
            .into()));

        fxa.set_client(Arc::new(client));
        assert!(fxa.devices_cache.is_none());

        let res = fxa.get_devices(false);

        assert!(res.is_err());
        assert!(fxa.devices_cache.is_none());
    }
}
