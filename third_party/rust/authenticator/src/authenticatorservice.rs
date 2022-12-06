/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::consts::PARAMETER_SIZE;
use crate::ctap2::commands::client_pin::Pin;
pub use crate::ctap2::commands::get_assertion::{
    GetAssertionExtensions, GetAssertionOptions, HmacSecretExtension,
};
pub use crate::ctap2::commands::make_credentials::{
    MakeCredentialsExtensions, MakeCredentialsOptions,
};
use crate::ctap2::server::{
    PublicKeyCredentialDescriptor, PublicKeyCredentialParameters, RelyingParty, User,
};
use crate::errors::*;
use crate::manager::Manager;
use crate::statecallback::StateCallback;
use std::sync::{mpsc::Sender, Arc, Mutex};

// TODO(MS): Once U2FManager gets completely removed, this can be removed as well
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CtapVersion {
    CTAP1,
    CTAP2,
}

#[derive(Debug, Clone)]
pub struct RegisterArgsCtap1 {
    pub flags: crate::RegisterFlags,
    pub challenge: Vec<u8>,
    pub application: crate::AppId,
    pub key_handles: Vec<crate::KeyHandle>,
}

#[derive(Debug, Clone)]
pub struct RegisterArgsCtap2 {
    pub challenge: Vec<u8>,
    pub relying_party: RelyingParty,
    pub origin: String,
    pub user: User,
    pub pub_cred_params: Vec<PublicKeyCredentialParameters>,
    pub exclude_list: Vec<PublicKeyCredentialDescriptor>,
    pub options: MakeCredentialsOptions,
    pub extensions: MakeCredentialsExtensions,
    pub pin: Option<Pin>,
}

#[derive(Debug)]
pub enum RegisterArgs {
    CTAP1(RegisterArgsCtap1),
    CTAP2(RegisterArgsCtap2),
}

impl From<RegisterArgsCtap1> for RegisterArgs {
    fn from(args: RegisterArgsCtap1) -> Self {
        RegisterArgs::CTAP1(args)
    }
}

impl From<RegisterArgsCtap2> for RegisterArgs {
    fn from(args: RegisterArgsCtap2) -> Self {
        RegisterArgs::CTAP2(args)
    }
}

#[derive(Debug, Clone)]
pub struct SignArgsCtap1 {
    pub flags: crate::SignFlags,
    pub challenge: Vec<u8>,
    pub app_ids: Vec<crate::AppId>,
    pub key_handles: Vec<crate::KeyHandle>,
}

#[derive(Debug, Clone)]
pub struct SignArgsCtap2 {
    pub challenge: Vec<u8>,
    pub origin: String,
    pub relying_party_id: String,
    pub allow_list: Vec<PublicKeyCredentialDescriptor>,
    pub options: GetAssertionOptions,
    pub extensions: GetAssertionExtensions,
    pub pin: Option<Pin>,
    // Todo: Extensions
}

#[derive(Debug)]
pub enum SignArgs {
    CTAP1(SignArgsCtap1),
    CTAP2(SignArgsCtap2),
}

impl From<SignArgsCtap1> for SignArgs {
    fn from(args: SignArgsCtap1) -> Self {
        SignArgs::CTAP1(args)
    }
}

impl From<SignArgsCtap2> for SignArgs {
    fn from(args: SignArgsCtap2) -> Self {
        SignArgs::CTAP2(args)
    }
}

#[derive(Debug, Clone, Default)]
pub struct AssertionExtensions {
    pub hmac_secret: Option<HmacSecretExtension>,
}

pub trait AuthenticatorTransport {
    /// The implementation of this method must return quickly and should
    /// report its status via the status and callback methods
    fn register(
        &mut self,
        timeout: u64,
        ctap_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()>;

    /// The implementation of this method must return quickly and should
    /// report its status via the status and callback methods
    fn sign(
        &mut self,
        timeout: u64,
        ctap_args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()>;

    fn cancel(&mut self) -> crate::Result<()>;
    fn reset(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()>;
    fn set_pin(
        &mut self,
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()>;
}

pub struct AuthenticatorService {
    transports: Vec<Arc<Mutex<Box<dyn AuthenticatorTransport + Send>>>>,
    ctap_version: CtapVersion,
}

fn clone_and_configure_cancellation_callback<T>(
    mut callback: StateCallback<T>,
    transports_to_cancel: Vec<Arc<Mutex<Box<dyn AuthenticatorTransport + Send>>>>,
) -> StateCallback<T> {
    callback.add_uncloneable_observer(Box::new(move || {
        debug!(
            "Callback observer is running, cancelling \
             {} unchosen transports...",
            transports_to_cancel.len()
        );
        for transport_mutex in &transports_to_cancel {
            if let Err(e) = transport_mutex.lock().unwrap().cancel() {
                error!("Cancellation failed: {:?}", e);
            }
        }
    }));
    callback
}

impl AuthenticatorService {
    pub fn new(ctap_version: CtapVersion) -> crate::Result<Self> {
        Ok(Self {
            transports: Vec::new(),
            ctap_version,
        })
    }

    /// Add any detected platform transports
    pub fn add_detected_transports(&mut self) {
        self.add_u2f_usb_hid_platform_transports();
    }

    fn add_transport(&mut self, boxed_token: Box<dyn AuthenticatorTransport + Send>) {
        self.transports.push(Arc::new(Mutex::new(boxed_token)))
    }

    pub fn add_u2f_usb_hid_platform_transports(&mut self) {
        if self.ctap_version == CtapVersion::CTAP1 {
            match crate::U2FManager::new() {
                Ok(token) => self.add_transport(Box::new(token)),
                Err(e) => error!("Could not add U2F HID transport: {}", e),
            }
        } else {
            match Manager::new() {
                Ok(token) => self.add_transport(Box::new(token)),
                Err(e) => error!("Could not add CTAP2 HID transport: {}", e),
            }
        }
    }

    #[cfg(feature = "webdriver")]
    pub fn add_webdriver_virtual_bus(&mut self) {
        match crate::virtualdevices::webdriver::VirtualManager::new() {
            Ok(token) => {
                println!("WebDriver ready, listening at {}", &token.url());
                self.add_transport(Box::new(token));
            }
            Err(e) => error!("Could not add WebDriver virtual bus: {}", e),
        }
    }

    pub fn register(
        &mut self,
        timeout: u64,
        ctap_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        match ctap_args {
            RegisterArgs::CTAP1(a) => self.register_ctap1(timeout, a, status, callback),
            RegisterArgs::CTAP2(a) => self.register_ctap2(timeout, a, status, callback),
        }
    }

    fn register_ctap1(
        &mut self,
        timeout: u64,
        args: RegisterArgsCtap1,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        if args.challenge.len() != PARAMETER_SIZE || args.application.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for key_handle in &args.key_handles {
            if key_handle.credential.len() >= 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        debug!(
            "register called with {} transports, iterable is {}",
            self.transports.len(),
            iterable_transports.len()
        );

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            debug!(
                "register transports_to_cancel {}",
                transports_to_cancel.len()
            );

            transport_mutex.lock().unwrap().register(
                timeout,
                RegisterArgs::CTAP1(args.clone()),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    fn register_ctap2(
        &mut self,
        timeout: u64,
        args: RegisterArgsCtap2,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        debug!(
            "register called with {} transports, iterable is {}",
            self.transports.len(),
            iterable_transports.len()
        );

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            debug!(
                "register transports_to_cancel {}",
                transports_to_cancel.len()
            );

            transport_mutex.lock().unwrap().register(
                timeout,
                RegisterArgs::CTAP2(args.clone()),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        ctap_args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        match ctap_args {
            SignArgs::CTAP1(a) => self.sign_ctap1(timeout, a, status, callback),
            SignArgs::CTAP2(a) => self.sign_ctap2(timeout, a, status, callback),
        }
    }

    pub fn sign_ctap1(
        &mut self,
        timeout: u64,
        args: SignArgsCtap1,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        if args.challenge.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        if args.app_ids.is_empty() {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for app_id in &args.app_ids {
            if app_id.len() != PARAMETER_SIZE {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        for key_handle in &args.key_handles {
            if key_handle.credential.len() >= 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            transport_mutex.lock().unwrap().sign(
                timeout,
                SignArgs::CTAP1(args.clone()),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn sign_ctap2(
        &mut self,
        timeout: u64,
        args: SignArgsCtap2,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            transport_mutex.lock().unwrap().sign(
                timeout,
                SignArgs::CTAP2(args.clone()),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn cancel(&mut self) -> crate::Result<()> {
        if self.transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        for transport_mutex in &mut self.transports {
            transport_mutex.lock().unwrap().cancel()?;
        }

        Ok(())
    }

    pub fn reset(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()> {
        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        debug!(
            "reset called with {} transports, iterable is {}",
            self.transports.len(),
            iterable_transports.len()
        );

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            debug!("reset transports_to_cancel {}", transports_to_cancel.len());

            transport_mutex.lock().unwrap().reset(
                timeout,
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn set_pin(
        &mut self,
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()> {
        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        debug!(
            "reset called with {} transports, iterable is {}",
            self.transports.len(),
            iterable_transports.len()
        );

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            debug!("reset transports_to_cancel {}", transports_to_cancel.len());

            transport_mutex.lock().unwrap().set_pin(
                timeout,
                new_pin.clone(),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use super::{
        AuthenticatorService, AuthenticatorTransport, CtapVersion, Pin,
        PublicKeyCredentialDescriptor, RegisterArgs, RegisterArgsCtap1, RegisterArgsCtap2,
        SignArgs, SignArgsCtap1, SignArgsCtap2, User,
    };
    use crate::consts::Capability;
    use crate::consts::PARAMETER_SIZE;
    use crate::ctap2::server::RelyingParty;
    use crate::statecallback::StateCallback;
    use crate::{AuthenticatorTransports, KeyHandle, RegisterFlags, SignFlags, StatusUpdate};
    use crate::{RegisterResult, SignResult};
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::mpsc::{channel, Sender};
    use std::sync::Arc;
    use std::{io, thread};

    fn init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    pub struct TestTransportDriver {
        consent: bool,
        was_cancelled: Arc<AtomicBool>,
    }

    impl TestTransportDriver {
        pub fn new(consent: bool) -> io::Result<Self> {
            Ok(Self {
                consent,
                was_cancelled: Arc::new(AtomicBool::new(false)),
            })
        }
    }

    impl TestTransportDriver {
        fn dev_info(&self) -> crate::u2ftypes::U2FDeviceInfo {
            crate::u2ftypes::U2FDeviceInfo {
                vendor_name: String::from("Mozilla").into_bytes(),
                device_name: String::from("Test Transport Token").into_bytes(),
                version_interface: 0,
                version_major: 1,
                version_minor: 2,
                version_build: 3,
                cap_flags: Capability::empty(),
            }
        }
    }

    impl AuthenticatorTransport for TestTransportDriver {
        fn register(
            &mut self,
            _timeout: u64,
            _args: RegisterArgs,
            _status: Sender<crate::StatusUpdate>,
            callback: StateCallback<crate::Result<crate::RegisterResult>>,
        ) -> crate::Result<()> {
            if self.consent {
                let rv = Ok(RegisterResult::CTAP1(vec![0u8; 16], self.dev_info()));
                thread::spawn(move || callback.call(rv));
            }
            Ok(())
        }

        fn sign(
            &mut self,
            _timeout: u64,
            _ctap_args: SignArgs,
            _status: Sender<crate::StatusUpdate>,
            callback: StateCallback<crate::Result<crate::SignResult>>,
        ) -> crate::Result<()> {
            if self.consent {
                let rv = Ok(SignResult::CTAP1(
                    vec![0u8; 0],
                    vec![0u8; 0],
                    vec![0u8; 0],
                    self.dev_info(),
                ));
                thread::spawn(move || callback.call(rv));
            }
            Ok(())
        }

        fn cancel(&mut self) -> crate::Result<()> {
            self.was_cancelled
                .compare_exchange(false, true, Ordering::SeqCst, Ordering::SeqCst)
                .map_or(
                    Err(crate::errors::AuthenticatorError::U2FToken(
                        crate::errors::U2FTokenError::InvalidState,
                    )),
                    |_| Ok(()),
                )
        }

        fn reset(
            &mut self,
            _timeout: u64,
            _status: Sender<crate::StatusUpdate>,
            _callback: StateCallback<crate::Result<crate::ResetResult>>,
        ) -> crate::Result<()> {
            unimplemented!();
        }

        fn set_pin(
            &mut self,
            _timeout: u64,
            _new_pin: Pin,
            _status: Sender<crate::StatusUpdate>,
            _callback: StateCallback<crate::Result<crate::ResetResult>>,
        ) -> crate::Result<()> {
            unimplemented!();
        }
    }

    fn mk_key() -> KeyHandle {
        KeyHandle {
            credential: vec![0],
            transports: AuthenticatorTransports::USB,
        }
    }

    fn mk_challenge() -> Vec<u8> {
        vec![0x11; PARAMETER_SIZE]
    }

    fn mk_appid() -> Vec<u8> {
        vec![0x22; PARAMETER_SIZE]
    }

    #[test]
    fn test_no_challenge() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: vec![],
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![mk_key()],
                }
                .into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                1_000,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: vec![],
                    app_ids: vec![mk_appid()],
                    key_handles: vec![mk_key()]
                }
                .into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );
    }

    #[test]
    fn test_no_appids() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: vec![],
                    key_handles: vec![mk_key()],
                }
                .into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                1_000,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: mk_challenge(),
                    app_ids: vec![],
                    key_handles: vec![mk_key()]
                }
                .into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );
    }

    #[test]
    fn test_no_keys() {
        init();
        // No Keys is a resident-key use case. For U2F this would time out,
        // but the actual reactions are up to the service implementation.
        // This test yields OKs.
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                100,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![],
                }
                .into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            ),
            Ok(())
        );

        assert_matches!(
            s.sign(
                100,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: mk_challenge(),
                    app_ids: vec![mk_appid()],
                    key_handles: vec![]
                }
                .into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            ),
            Ok(())
        );
    }

    #[test]
    fn test_large_keys() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let large_key = KeyHandle {
            credential: vec![0; 257],
            transports: AuthenticatorTransports::USB,
        };

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![large_key.clone()],
                }
                .into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                1_000,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: mk_challenge(),
                    app_ids: vec![mk_appid()],
                    key_handles: vec![large_key]
                }
                .into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );
    }

    #[test]
    fn test_large_keys_ctap2() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let large_key = KeyHandle {
            credential: vec![0; 1000],
            transports: AuthenticatorTransports::USB,
        };

        let mut s = AuthenticatorService::new(CtapVersion::CTAP2).unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        let ctap2_register_args = RegisterArgsCtap2 {
            challenge: mk_challenge(),
            relying_party: RelyingParty {
                id: "example.com".to_string(),
                name: None,
                icon: None,
            },
            origin: "example.com".to_string(),
            user: User {
                id: "user_id".as_bytes().to_vec(),
                icon: None,
                name: Some("A. User".to_string()),
                display_name: None,
            },
            pub_cred_params: vec![],
            exclude_list: vec![(&large_key).into()],
            options: Default::default(),
            extensions: Default::default(),
            pin: None,
        };

        assert!(s
            .register(
                1_000,
                ctap2_register_args.into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .is_ok(),);

        let ctap2_sign_args = SignArgsCtap2 {
            challenge: mk_challenge(),
            origin: "example.com".to_string(),
            relying_party_id: "example.com".to_string(),
            allow_list: vec![(&large_key).into()],
            options: Default::default(),
            extensions: Default::default(),
            pin: None,
        };
        assert!(s
            .sign(
                1_000,
                ctap2_sign_args.into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .is_ok(),);
    }

    #[test]
    fn test_no_transports() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        assert_matches!(
            s.register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![mk_key()],
                }
                .into(),
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::NoConfiguredTransports
        );

        assert_matches!(
            s.sign(
                1_000,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: mk_challenge(),
                    app_ids: vec![mk_appid()],
                    key_handles: vec![mk_key()]
                }
                .into(),
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::NoConfiguredTransports
        );

        assert_matches!(
            s.cancel().unwrap_err(),
            crate::errors::AuthenticatorError::NoConfiguredTransports
        );
    }

    #[test]
    fn test_cancellation_register() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        let ttd_one = TestTransportDriver::new(true).unwrap();
        let ttd_two = TestTransportDriver::new(false).unwrap();
        let ttd_three = TestTransportDriver::new(false).unwrap();

        let was_cancelled_one = ttd_one.was_cancelled.clone();
        let was_cancelled_two = ttd_two.was_cancelled.clone();
        let was_cancelled_three = ttd_three.was_cancelled.clone();

        s.add_transport(Box::new(ttd_one));
        s.add_transport(Box::new(ttd_two));
        s.add_transport(Box::new(ttd_three));

        let callback = StateCallback::new(Box::new(move |_rv| {}));
        assert!(s
            .register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![],
                }
                .into(),
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        assert_eq!(was_cancelled_one.load(Ordering::SeqCst), false);
        assert_eq!(was_cancelled_two.load(Ordering::SeqCst), true);
        assert_eq!(was_cancelled_three.load(Ordering::SeqCst), true);
    }

    #[test]
    fn test_cancellation_sign() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        let ttd_one = TestTransportDriver::new(true).unwrap();
        let ttd_two = TestTransportDriver::new(false).unwrap();
        let ttd_three = TestTransportDriver::new(false).unwrap();

        let was_cancelled_one = ttd_one.was_cancelled.clone();
        let was_cancelled_two = ttd_two.was_cancelled.clone();
        let was_cancelled_three = ttd_three.was_cancelled.clone();

        s.add_transport(Box::new(ttd_one));
        s.add_transport(Box::new(ttd_two));
        s.add_transport(Box::new(ttd_three));

        let callback = StateCallback::new(Box::new(move |_rv| {}));
        assert!(s
            .sign(
                1_000,
                SignArgsCtap1 {
                    flags: SignFlags::empty(),
                    challenge: mk_challenge(),
                    app_ids: vec![mk_appid()],
                    key_handles: vec![mk_key()]
                }
                .into(),
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        assert_eq!(was_cancelled_one.load(Ordering::SeqCst), false);
        assert_eq!(was_cancelled_two.load(Ordering::SeqCst), true);
        assert_eq!(was_cancelled_three.load(Ordering::SeqCst), true);
    }

    #[test]
    fn test_cancellation_race() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new(CtapVersion::CTAP1).unwrap();
        // Let both of these race which one provides consent.
        let ttd_one = TestTransportDriver::new(true).unwrap();
        let ttd_two = TestTransportDriver::new(true).unwrap();

        let was_cancelled_one = ttd_one.was_cancelled.clone();
        let was_cancelled_two = ttd_two.was_cancelled.clone();

        s.add_transport(Box::new(ttd_one));
        s.add_transport(Box::new(ttd_two));

        let callback = StateCallback::new(Box::new(move |_rv| {}));
        assert!(s
            .register(
                1_000,
                RegisterArgsCtap1 {
                    challenge: mk_challenge(),
                    flags: RegisterFlags::empty(),
                    application: mk_appid(),
                    key_handles: vec![],
                }
                .into(),
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        let one = was_cancelled_one.load(Ordering::SeqCst);
        let two = was_cancelled_two.load(Ordering::SeqCst);
        assert_eq!(
            one ^ two,
            true,
            "asserting that one={} xor two={} is true",
            one,
            two
        );
    }
}
