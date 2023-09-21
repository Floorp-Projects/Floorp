/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ctap2::commands::client_pin::Pin;
use crate::ctap2::server::{
    AuthenticationExtensionsClientInputs, PublicKeyCredentialDescriptor,
    PublicKeyCredentialParameters, PublicKeyCredentialUserEntity, RelyingParty,
    ResidentKeyRequirement, UserVerificationRequirement,
};
use crate::errors::*;
use crate::manager::Manager;
use crate::statecallback::StateCallback;
use std::sync::{mpsc::Sender, Arc, Mutex};

#[derive(Debug, Clone)]
pub struct RegisterArgs {
    pub client_data_hash: [u8; 32],
    pub relying_party: RelyingParty,
    pub origin: String,
    pub user: PublicKeyCredentialUserEntity,
    pub pub_cred_params: Vec<PublicKeyCredentialParameters>,
    pub exclude_list: Vec<PublicKeyCredentialDescriptor>,
    pub user_verification_req: UserVerificationRequirement,
    pub resident_key_req: ResidentKeyRequirement,
    pub extensions: AuthenticationExtensionsClientInputs,
    pub pin: Option<Pin>,
    pub use_ctap1_fallback: bool,
}

#[derive(Debug, Clone)]
pub struct SignArgs {
    pub client_data_hash: [u8; 32],
    pub origin: String,
    pub relying_party_id: String,
    pub allow_list: Vec<PublicKeyCredentialDescriptor>,
    pub user_verification_req: UserVerificationRequirement,
    pub user_presence_req: bool,
    pub extensions: AuthenticationExtensionsClientInputs,
    pub pin: Option<Pin>,
    pub use_ctap1_fallback: bool,
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
    fn manage(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ManageResult>>,
    ) -> crate::Result<()>;
}

pub struct AuthenticatorService {
    transports: Vec<Arc<Mutex<Box<dyn AuthenticatorTransport + Send>>>>,
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
    pub fn new() -> crate::Result<Self> {
        Ok(Self {
            transports: Vec::new(),
        })
    }

    /// Add any detected platform transports
    pub fn add_detected_transports(&mut self) {
        self.add_u2f_usb_hid_platform_transports();
    }

    pub fn add_transport(&mut self, boxed_token: Box<dyn AuthenticatorTransport + Send>) {
        self.transports.push(Arc::new(Mutex::new(boxed_token)))
    }

    pub fn add_u2f_usb_hid_platform_transports(&mut self) {
        match Manager::new() {
            Ok(token) => self.add_transport(Box::new(token)),
            Err(e) => error!("Could not add CTAP2 HID transport: {}", e),
        }
    }

    pub fn register(
        &mut self,
        timeout: u64,
        args: RegisterArgs,
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
                args.clone(),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        args: SignArgs,
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
                args.clone(),
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

    pub fn manage(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ManageResult>>,
    ) -> crate::Result<()> {
        let iterable_transports = self.transports.clone();
        if iterable_transports.is_empty() {
            return Err(AuthenticatorError::NoConfiguredTransports);
        }

        debug!(
            "Manage called with {} transports, iterable is {}",
            self.transports.len(),
            iterable_transports.len()
        );

        for (idx, transport_mutex) in iterable_transports.iter().enumerate() {
            let mut transports_to_cancel = iterable_transports.clone();
            transports_to_cancel.remove(idx);

            debug!("reset transports_to_cancel {}", transports_to_cancel.len());

            transport_mutex.lock().unwrap().manage(
                timeout,
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
    use super::{AuthenticatorService, AuthenticatorTransport, Pin, RegisterArgs, SignArgs};
    use crate::consts::PARAMETER_SIZE;
    use crate::ctap2::server::{
        PublicKeyCredentialUserEntity, RelyingParty, ResidentKeyRequirement,
        UserVerificationRequirement,
    };
    use crate::errors::AuthenticatorError;
    use crate::statecallback::StateCallback;
    use crate::StatusUpdate;
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

    impl AuthenticatorTransport for TestTransportDriver {
        fn register(
            &mut self,
            _timeout: u64,
            _args: RegisterArgs,
            _status: Sender<crate::StatusUpdate>,
            callback: StateCallback<crate::Result<crate::RegisterResult>>,
        ) -> crate::Result<()> {
            if self.consent {
                // The value we send is ignored, and this is easier than constructing a
                // RegisterResult
                let rv = Err(AuthenticatorError::Platform);
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
                // The value we send is ignored, and this is easier than constructing a
                // RegisterResult
                let rv = Err(AuthenticatorError::Platform);
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

        fn manage(
            &mut self,
            _timeout: u64,
            _status: Sender<crate::StatusUpdate>,
            _callback: StateCallback<crate::Result<crate::ManageResult>>,
        ) -> crate::Result<()> {
            unimplemented!();
        }
    }

    fn mk_challenge() -> [u8; PARAMETER_SIZE] {
        [0x11; PARAMETER_SIZE]
    }

    #[test]
    fn test_no_transports() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new().unwrap();
        assert_matches!(
            s.register(
                1_000,
                RegisterArgs {
                    client_data_hash: mk_challenge(),
                    relying_party: RelyingParty {
                        id: "example.com".to_string(),
                        name: None,
                    },
                    origin: "example.com".to_string(),
                    user: PublicKeyCredentialUserEntity {
                        id: "user_id".as_bytes().to_vec(),
                        name: Some("A. User".to_string()),
                        display_name: None,
                    },
                    pub_cred_params: vec![],
                    exclude_list: vec![],
                    user_verification_req: UserVerificationRequirement::Preferred,
                    resident_key_req: ResidentKeyRequirement::Preferred,
                    extensions: Default::default(),
                    pin: None,
                    use_ctap1_fallback: false,
                },
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::NoConfiguredTransports
        );

        assert_matches!(
            s.sign(
                1_000,
                SignArgs {
                    client_data_hash: mk_challenge(),
                    origin: "example.com".to_string(),
                    relying_party_id: "example.com".to_string(),
                    allow_list: vec![],
                    user_verification_req: UserVerificationRequirement::Preferred,
                    user_presence_req: true,
                    extensions: Default::default(),
                    pin: None,
                    use_ctap1_fallback: false,
                },
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

        let mut s = AuthenticatorService::new().unwrap();
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
                RegisterArgs {
                    client_data_hash: mk_challenge(),
                    relying_party: RelyingParty {
                        id: "example.com".to_string(),
                        name: None,
                    },
                    origin: "example.com".to_string(),
                    user: PublicKeyCredentialUserEntity {
                        id: "user_id".as_bytes().to_vec(),
                        name: Some("A. User".to_string()),
                        display_name: None,
                    },
                    pub_cred_params: vec![],
                    exclude_list: vec![],
                    user_verification_req: UserVerificationRequirement::Preferred,
                    resident_key_req: ResidentKeyRequirement::Preferred,
                    extensions: Default::default(),
                    pin: None,
                    use_ctap1_fallback: false,
                },
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        assert!(!was_cancelled_one.load(Ordering::SeqCst));
        assert!(was_cancelled_two.load(Ordering::SeqCst));
        assert!(was_cancelled_three.load(Ordering::SeqCst));
    }

    #[test]
    fn test_cancellation_sign() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new().unwrap();
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
                SignArgs {
                    client_data_hash: mk_challenge(),
                    origin: "example.com".to_string(),
                    relying_party_id: "example.com".to_string(),
                    allow_list: vec![],
                    user_verification_req: UserVerificationRequirement::Preferred,
                    user_presence_req: true,
                    extensions: Default::default(),
                    pin: None,
                    use_ctap1_fallback: false,
                },
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        assert!(!was_cancelled_one.load(Ordering::SeqCst));
        assert!(was_cancelled_two.load(Ordering::SeqCst));
        assert!(was_cancelled_three.load(Ordering::SeqCst));
    }

    #[test]
    fn test_cancellation_race() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new().unwrap();
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
                RegisterArgs {
                    client_data_hash: mk_challenge(),
                    relying_party: RelyingParty {
                        id: "example.com".to_string(),
                        name: None,
                    },
                    origin: "example.com".to_string(),
                    user: PublicKeyCredentialUserEntity {
                        id: "user_id".as_bytes().to_vec(),
                        name: Some("A. User".to_string()),
                        display_name: None,
                    },
                    pub_cred_params: vec![],
                    exclude_list: vec![],
                    user_verification_req: UserVerificationRequirement::Preferred,
                    resident_key_req: ResidentKeyRequirement::Preferred,
                    extensions: Default::default(),
                    pin: None,
                    use_ctap1_fallback: false,
                },
                status_tx,
                callback.clone(),
            )
            .is_ok());
        callback.wait();

        let one = was_cancelled_one.load(Ordering::SeqCst);
        let two = was_cancelled_two.load(Ordering::SeqCst);
        assert!(
            one ^ two,
            "asserting that one={} xor two={} is true",
            one,
            two
        );
    }
}
