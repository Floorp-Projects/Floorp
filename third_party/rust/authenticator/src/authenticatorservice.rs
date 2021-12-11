/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::{mpsc::Sender, Arc, Mutex};

use crate::consts::PARAMETER_SIZE;
use crate::errors::*;
use crate::statecallback::StateCallback;

pub trait AuthenticatorTransport {
    /// The implementation of this method must return quickly and should
    /// report its status via the status and callback methods
    fn register(
        &mut self,
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: crate::AppId,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()>;

    /// The implementation of this method must return quickly and should
    /// report its status via the status and callback methods
    fn sign(
        &mut self,
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()>;

    fn cancel(&mut self) -> crate::Result<()>;
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

    fn add_transport(&mut self, boxed_token: Box<dyn AuthenticatorTransport + Send>) {
        self.transports.push(Arc::new(Mutex::new(boxed_token)))
    }

    pub fn add_u2f_usb_hid_platform_transports(&mut self) {
        match crate::U2FManager::new() {
            Ok(token) => self.add_transport(Box::new(token)),
            Err(e) => error!("Could not add U2F HID transport: {}", e),
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
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: crate::AppId,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
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
                flags,
                timeout,
                challenge.clone(),
                application.clone(),
                key_handles.clone(),
                status.clone(),
                clone_and_configure_cancellation_callback(callback.clone(), transports_to_cancel),
            )?;
        }

        Ok(())
    }

    pub fn sign(
        &mut self,
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        if challenge.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        if app_ids.is_empty() {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for app_id in &app_ids {
            if app_id.len() != PARAMETER_SIZE {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
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
                flags,
                timeout,
                challenge.clone(),
                app_ids.clone(),
                key_handles.clone(),
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
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use super::{AuthenticatorService, AuthenticatorTransport};
    use crate::consts::PARAMETER_SIZE;
    use crate::statecallback::StateCallback;
    use crate::{AuthenticatorTransports, KeyHandle, RegisterFlags, SignFlags, StatusUpdate};
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
                cap_flags: 0,
            }
        }
    }

    impl AuthenticatorTransport for TestTransportDriver {
        fn register(
            &mut self,
            _flags: crate::RegisterFlags,
            _timeout: u64,
            _challenge: Vec<u8>,
            _application: crate::AppId,
            _key_handles: Vec<crate::KeyHandle>,
            _status: Sender<crate::StatusUpdate>,
            callback: StateCallback<crate::Result<crate::RegisterResult>>,
        ) -> crate::Result<()> {
            if self.consent {
                let rv = Ok((vec![0u8; 16], self.dev_info()));
                thread::spawn(move || callback.call(rv));
            }
            Ok(())
        }

        fn sign(
            &mut self,
            _flags: crate::SignFlags,
            _timeout: u64,
            _challenge: Vec<u8>,
            _app_ids: Vec<crate::AppId>,
            _key_handles: Vec<crate::KeyHandle>,
            _status: Sender<crate::StatusUpdate>,
            callback: StateCallback<crate::Result<crate::SignResult>>,
        ) -> crate::Result<()> {
            if self.consent {
                let rv = Ok((vec![0u8; 0], vec![0u8; 0], vec![0u8; 0], self.dev_info()));
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

        let mut s = AuthenticatorService::new().unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                RegisterFlags::empty(),
                1_000,
                vec![],
                mk_appid(),
                vec![mk_key()],
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                SignFlags::empty(),
                1_000,
                vec![],
                vec![mk_appid()],
                vec![mk_key()],
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

        let mut s = AuthenticatorService::new().unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                RegisterFlags::empty(),
                1_000,
                mk_challenge(),
                vec![],
                vec![mk_key()],
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                SignFlags::empty(),
                1_000,
                mk_challenge(),
                vec![],
                vec![mk_key()],
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

        let mut s = AuthenticatorService::new().unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                RegisterFlags::empty(),
                100,
                mk_challenge(),
                mk_appid(),
                vec![],
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            ),
            Ok(())
        );

        assert_matches!(
            s.sign(
                SignFlags::empty(),
                100,
                mk_challenge(),
                vec![mk_appid()],
                vec![],
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

        let mut s = AuthenticatorService::new().unwrap();
        s.add_transport(Box::new(TestTransportDriver::new(true).unwrap()));

        assert_matches!(
            s.register(
                RegisterFlags::empty(),
                1_000,
                mk_challenge(),
                mk_appid(),
                vec![large_key.clone()],
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );

        assert_matches!(
            s.sign(
                SignFlags::empty(),
                1_000,
                mk_challenge(),
                vec![mk_appid()],
                vec![large_key],
                status_tx,
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::InvalidRelyingPartyInput
        );
    }

    #[test]
    fn test_no_transports() {
        init();
        let (status_tx, _) = channel::<StatusUpdate>();

        let mut s = AuthenticatorService::new().unwrap();
        assert_matches!(
            s.register(
                RegisterFlags::empty(),
                1_000,
                mk_challenge(),
                mk_appid(),
                vec![mk_key()],
                status_tx.clone(),
                StateCallback::new(Box::new(move |_rv| {})),
            )
            .unwrap_err(),
            crate::errors::AuthenticatorError::NoConfiguredTransports
        );

        assert_matches!(
            s.sign(
                SignFlags::empty(),
                1_000,
                mk_challenge(),
                vec![mk_appid()],
                vec![mk_key()],
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
                RegisterFlags::empty(),
                1_000,
                mk_challenge(),
                mk_appid(),
                vec![],
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
                SignFlags::empty(),
                1_000,
                mk_challenge(),
                vec![mk_appid()],
                vec![mk_key()],
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
                RegisterFlags::empty(),
                1_000,
                mk_challenge(),
                mk_appid(),
                vec![],
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
