/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use pkcs11_bindings::*;
use std::collections::{BTreeMap, BTreeSet};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};

use crate::error::{Error, ErrorType};
use crate::error_here;
use crate::util::*;

/// Helper enum to differentiate between sessions on the modern slot and sessions on the legacy
/// slot. The former is for EC keys and RSA keys that can be used with RSA-PSS whereas the latter is
/// for RSA keys that cannot be used with RSA-PSS.
#[derive(Clone, Copy, PartialEq)]
pub enum SlotType {
    Modern,
    Legacy,
}

pub trait CryptokiObject {
    fn matches(&self, slot_type: SlotType, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool;
    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]>;
}

pub trait Sign {
    fn get_signature_length(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, Error>;
    fn sign(
        &mut self,
        data: &[u8],
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, Error>;
}

pub trait ClientCertsBackend {
    type Cert: CryptokiObject;
    type Key: CryptokiObject + Sign;

    #[allow(clippy::type_complexity)]
    fn find_objects(&self) -> Result<(Vec<Self::Cert>, Vec<Self::Key>), Error>;
}

/// Helper type for sending `ManagerArguments` to the real `Manager`.
type ManagerArgumentsSender = Sender<ManagerArguments>;
/// Helper type for receiving `ManagerReturnValue`s from the real `Manager`.
type ManagerReturnValueReceiver = Receiver<ManagerReturnValue>;

/// Helper enum that encapsulates arguments to send from the `ManagerProxy` to the real `Manager`.
/// `ManagerArguments::Stop` is a special variant that stops the background thread and drops the
/// `Manager`.
enum ManagerArguments {
    OpenSession(SlotType),
    CloseSession(CK_SESSION_HANDLE),
    CloseAllSessions(SlotType),
    StartSearch(CK_SESSION_HANDLE, Vec<(CK_ATTRIBUTE_TYPE, Vec<u8>)>),
    Search(CK_SESSION_HANDLE, usize),
    ClearSearch(CK_SESSION_HANDLE),
    GetAttributes(CK_OBJECT_HANDLE, Vec<CK_ATTRIBUTE_TYPE>),
    StartSign(
        CK_SESSION_HANDLE,
        CK_OBJECT_HANDLE,
        Option<CK_RSA_PKCS_PSS_PARAMS>,
    ),
    GetSignatureLength(CK_SESSION_HANDLE, Vec<u8>),
    Sign(CK_SESSION_HANDLE, Vec<u8>),
    Stop,
}

/// Helper enum that encapsulates return values from the real `Manager` that are sent back to the
/// `ManagerProxy`. `ManagerReturnValue::Stop` is a special variant that indicates that the
/// `Manager` will stop.
enum ManagerReturnValue {
    OpenSession(Result<CK_SESSION_HANDLE, Error>),
    CloseSession(Result<(), Error>),
    CloseAllSessions(Result<(), Error>),
    StartSearch(Result<(), Error>),
    Search(Result<Vec<CK_OBJECT_HANDLE>, Error>),
    ClearSearch(Result<(), Error>),
    GetAttributes(Result<Vec<Option<Vec<u8>>>, Error>),
    StartSign(Result<(), Error>),
    GetSignatureLength(Result<usize, Error>),
    Sign(Result<Vec<u8>, Error>),
    Stop(Result<(), Error>),
}

/// Helper macro to implement the body of each public `ManagerProxy` function. Takes a
/// `ManagerProxy` instance (should always be `self`), a `ManagerArguments` representing the
/// `Manager` function to call and the arguments to use, and the qualified type of the expected
/// `ManagerReturnValue` that will be received from the `Manager` when it is done.
macro_rules! manager_proxy_fn_impl {
    ($manager:ident, $argument_enum:expr, $return_type:path) => {
        match $manager.proxy_call($argument_enum) {
            Ok($return_type(result)) => result,
            Ok(_) => Err(error_here!(ErrorType::LibraryFailure)),
            Err(e) => Err(e),
        }
    };
}

/// `ManagerProxy` synchronously proxies calls from any thread to the `Manager` that runs on a
/// single thread. This is necessary because the underlying OS APIs in use are not guaranteed to be
/// thread-safe (e.g. they may use thread-local storage). Using it should be identical to using the
/// real `Manager`.
pub struct ManagerProxy {
    sender: ManagerArgumentsSender,
    receiver: ManagerReturnValueReceiver,
    thread_handle: Option<JoinHandle<()>>,
}

impl ManagerProxy {
    pub fn new<B: ClientCertsBackend + Send + 'static>(backend: B) -> Result<ManagerProxy, Error> {
        let (proxy_sender, manager_receiver) = channel();
        let (manager_sender, proxy_receiver) = channel();
        let thread_handle = thread::Builder::new()
            .name("osclientcert".into())
            .spawn(move || {
                let mut real_manager = Manager::new(backend);
                while let Ok(arguments) = manager_receiver.recv() {
                    let results = match arguments {
                        ManagerArguments::OpenSession(slot_type) => {
                            ManagerReturnValue::OpenSession(real_manager.open_session(slot_type))
                        }
                        ManagerArguments::CloseSession(session_handle) => {
                            ManagerReturnValue::CloseSession(
                                real_manager.close_session(session_handle),
                            )
                        }
                        ManagerArguments::CloseAllSessions(slot_type) => {
                            ManagerReturnValue::CloseAllSessions(
                                real_manager.close_all_sessions(slot_type),
                            )
                        }
                        ManagerArguments::StartSearch(session, attrs) => {
                            ManagerReturnValue::StartSearch(
                                real_manager.start_search(session, attrs),
                            )
                        }
                        ManagerArguments::Search(session, max_objects) => {
                            ManagerReturnValue::Search(real_manager.search(session, max_objects))
                        }
                        ManagerArguments::ClearSearch(session) => {
                            ManagerReturnValue::ClearSearch(real_manager.clear_search(session))
                        }
                        ManagerArguments::GetAttributes(object_handle, attr_types) => {
                            ManagerReturnValue::GetAttributes(
                                real_manager.get_attributes(object_handle, attr_types),
                            )
                        }
                        ManagerArguments::StartSign(session, key_handle, params) => {
                            ManagerReturnValue::StartSign(
                                real_manager.start_sign(session, key_handle, params),
                            )
                        }
                        ManagerArguments::GetSignatureLength(session, data) => {
                            ManagerReturnValue::GetSignatureLength(
                                real_manager.get_signature_length(session, data),
                            )
                        }
                        ManagerArguments::Sign(session, data) => {
                            ManagerReturnValue::Sign(real_manager.sign(session, data))
                        }
                        ManagerArguments::Stop => ManagerReturnValue::Stop(Ok(())),
                    };
                    let stop_after_send = matches!(&results, &ManagerReturnValue::Stop(_));
                    match manager_sender.send(results) {
                        Ok(()) => {}
                        Err(_) => {
                            break;
                        }
                    }
                    if stop_after_send {
                        break;
                    }
                }
            });
        match thread_handle {
            Ok(thread_handle) => Ok(ManagerProxy {
                sender: proxy_sender,
                receiver: proxy_receiver,
                thread_handle: Some(thread_handle),
            }),
            Err(_) => Err(error_here!(ErrorType::LibraryFailure)),
        }
    }

    fn proxy_call(&self, args: ManagerArguments) -> Result<ManagerReturnValue, Error> {
        match self.sender.send(args) {
            Ok(()) => {}
            Err(_) => {
                return Err(error_here!(ErrorType::LibraryFailure));
            }
        };
        let result = match self.receiver.recv() {
            Ok(result) => result,
            Err(_) => {
                return Err(error_here!(ErrorType::LibraryFailure));
            }
        };
        Ok(result)
    }

    pub fn open_session(&mut self, slot_type: SlotType) -> Result<CK_SESSION_HANDLE, Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::OpenSession(slot_type),
            ManagerReturnValue::OpenSession
        )
    }

    pub fn close_session(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::CloseSession(session),
            ManagerReturnValue::CloseSession
        )
    }

    pub fn close_all_sessions(&mut self, slot_type: SlotType) -> Result<(), Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::CloseAllSessions(slot_type),
            ManagerReturnValue::CloseAllSessions
        )
    }

    pub fn start_search(
        &mut self,
        session: CK_SESSION_HANDLE,
        attrs: Vec<(CK_ATTRIBUTE_TYPE, Vec<u8>)>,
    ) -> Result<(), Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::StartSearch(session, attrs),
            ManagerReturnValue::StartSearch
        )
    }

    pub fn search(
        &mut self,
        session: CK_SESSION_HANDLE,
        max_objects: usize,
    ) -> Result<Vec<CK_OBJECT_HANDLE>, Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::Search(session, max_objects),
            ManagerReturnValue::Search
        )
    }

    pub fn clear_search(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::ClearSearch(session),
            ManagerReturnValue::ClearSearch
        )
    }

    pub fn get_attributes(
        &self,
        object_handle: CK_OBJECT_HANDLE,
        attr_types: Vec<CK_ATTRIBUTE_TYPE>,
    ) -> Result<Vec<Option<Vec<u8>>>, Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::GetAttributes(object_handle, attr_types,),
            ManagerReturnValue::GetAttributes
        )
    }

    pub fn start_sign(
        &mut self,
        session: CK_SESSION_HANDLE,
        key_handle: CK_OBJECT_HANDLE,
        params: Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<(), Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::StartSign(session, key_handle, params),
            ManagerReturnValue::StartSign
        )
    }

    pub fn get_signature_length(
        &self,
        session: CK_SESSION_HANDLE,
        data: Vec<u8>,
    ) -> Result<usize, Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::GetSignatureLength(session, data),
            ManagerReturnValue::GetSignatureLength
        )
    }

    pub fn sign(&mut self, session: CK_SESSION_HANDLE, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        manager_proxy_fn_impl!(
            self,
            ManagerArguments::Sign(session, data),
            ManagerReturnValue::Sign
        )
    }

    pub fn stop(&mut self) -> Result<(), Error> {
        manager_proxy_fn_impl!(self, ManagerArguments::Stop, ManagerReturnValue::Stop)?;
        let thread_handle = match self.thread_handle.take() {
            Some(thread_handle) => thread_handle,
            None => return Err(error_here!(ErrorType::LibraryFailure)),
        };
        thread_handle
            .join()
            .map_err(|_| error_here!(ErrorType::LibraryFailure))
    }
}

// Determines if the attributes of a given search correspond to NSS looking for all certificates or
// private keys. Returns true if so, and false otherwise.
// These searches are of the form:
//   { { type: CKA_TOKEN, value: [1] },
//     { type: CKA_CLASS, value: [CKO_CERTIFICATE or CKO_PRIVATE_KEY, as serialized bytes] } }
// (although not necessarily in that order - see nssToken_TraverseCertificates and
// nssToken_FindPrivateKeys)
fn search_is_for_all_certificates_or_keys(
    attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)],
) -> Result<bool, Error> {
    if attrs.len() != 2 {
        return Ok(false);
    }
    let token_bytes = vec![1_u8];
    let mut found_token = false;
    let cko_certificate_bytes = serialize_uint(CKO_CERTIFICATE)?;
    let cko_private_key_bytes = serialize_uint(CKO_PRIVATE_KEY)?;
    let mut found_certificate_or_private_key = false;
    for (attr_type, attr_value) in attrs.iter() {
        if attr_type == &CKA_TOKEN && attr_value == &token_bytes {
            found_token = true;
        }
        if attr_type == &CKA_CLASS
            && (attr_value == &cko_certificate_bytes || attr_value == &cko_private_key_bytes)
        {
            found_certificate_or_private_key = true;
        }
    }
    Ok(found_token && found_certificate_or_private_key)
}

const SUPPORTED_ATTRIBUTES: &[CK_ATTRIBUTE_TYPE] = &[
    CKA_CLASS,
    CKA_TOKEN,
    CKA_LABEL,
    CKA_ID,
    CKA_VALUE,
    CKA_ISSUER,
    CKA_SERIAL_NUMBER,
    CKA_SUBJECT,
    CKA_PRIVATE,
    CKA_KEY_TYPE,
    CKA_MODULUS,
    CKA_EC_PARAMS,
];

enum Object<B: ClientCertsBackend> {
    Cert(B::Cert),
    Key(B::Key),
}

impl<B: ClientCertsBackend> Object<B> {
    fn matches(&self, slot_type: SlotType, attrs: &[(CK_ATTRIBUTE_TYPE, Vec<u8>)]) -> bool {
        match self {
            Object::Cert(cert) => cert.matches(slot_type, attrs),
            Object::Key(key) => key.matches(slot_type, attrs),
        }
    }

    fn get_attribute(&self, attribute: CK_ATTRIBUTE_TYPE) -> Option<&[u8]> {
        match self {
            Object::Cert(cert) => cert.get_attribute(attribute),
            Object::Key(key) => key.get_attribute(attribute),
        }
    }

    fn id(&self) -> Result<&[u8], Error> {
        self.get_attribute(CKA_ID)
            .ok_or_else(|| error_here!(ErrorType::LibraryFailure))
    }

    fn get_signature_length(
        &mut self,
        data: Vec<u8>,
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<usize, Error> {
        match self {
            Object::Cert(_) => Err(error_here!(ErrorType::InvalidArgument)),
            Object::Key(key) => key.get_signature_length(&data, params),
        }
    }

    fn sign(
        &mut self,
        data: Vec<u8>,
        params: &Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<Vec<u8>, Error> {
        match self {
            Object::Cert(_) => Err(error_here!(ErrorType::InvalidArgument)),
            Object::Key(key) => key.sign(&data, params),
        }
    }
}

/// The `Manager` keeps track of the state of this module with respect to the PKCS #11
/// specification. This includes what sessions are open, which search and sign operations are
/// ongoing, and what objects are known and by what handle.
pub struct Manager<B: ClientCertsBackend> {
    /// A map of session to session type (modern or legacy). Sessions can be created (opened) and
    /// later closed.
    sessions: BTreeMap<CK_SESSION_HANDLE, SlotType>,
    /// A map of searches to PKCS #11 object handles that match those searches.
    searches: BTreeMap<CK_SESSION_HANDLE, Vec<CK_OBJECT_HANDLE>>,
    /// A map of sign operations to a pair of the object handle and optionally some params being
    /// used by each one.
    signs: BTreeMap<CK_SESSION_HANDLE, (CK_OBJECT_HANDLE, Option<CK_RSA_PKCS_PSS_PARAMS>)>,
    /// A map of object handles to the underlying objects.
    objects: BTreeMap<CK_OBJECT_HANDLE, Object<B>>,
    /// A set of certificate identifiers (not the same as handles).
    cert_ids: BTreeSet<Vec<u8>>,
    /// A set of key identifiers (not the same as handles). For each id in this set, there should be
    /// a corresponding identical id in the `cert_ids` set.
    key_ids: BTreeSet<Vec<u8>>,
    /// The next session handle to hand out.
    next_session: CK_SESSION_HANDLE,
    /// The next object handle to hand out.
    next_handle: CK_OBJECT_HANDLE,
    /// The last time the implementation looked for new objects in the backend.
    /// The implementation does this search no more than once every 3 seconds.
    last_scan_time: Option<Instant>,
    backend: B,
}

impl<B: ClientCertsBackend> Manager<B> {
    pub fn new(backend: B) -> Manager<B> {
        Manager {
            sessions: BTreeMap::new(),
            searches: BTreeMap::new(),
            signs: BTreeMap::new(),
            objects: BTreeMap::new(),
            cert_ids: BTreeSet::new(),
            key_ids: BTreeSet::new(),
            next_session: 1,
            next_handle: 1,
            last_scan_time: None,
            backend,
        }
    }

    /// When a new search session is opened (provided at least 3 seconds have elapsed since the
    /// last session was opened), this searches for certificates and keys to expose. We
    /// de-duplicate previously-found certificates and keys by keeping track of their IDs.
    fn maybe_find_new_objects(&mut self) -> Result<(), Error> {
        let now = Instant::now();
        match self.last_scan_time {
            Some(last_scan_time) => {
                if now.duration_since(last_scan_time) < Duration::new(3, 0) {
                    return Ok(());
                }
            }
            None => {}
        }
        self.last_scan_time = Some(now);
        let (certs, keys) = self.backend.find_objects()?;
        for cert in certs {
            let object = Object::Cert(cert);
            if self.cert_ids.contains(object.id()?) {
                continue;
            }
            self.cert_ids.insert(object.id()?.to_vec());
            let handle = self.get_next_handle();
            self.objects.insert(handle, object);
        }
        for key in keys {
            let object = Object::Key(key);
            if self.key_ids.contains(object.id()?) {
                continue;
            }
            self.key_ids.insert(object.id()?.to_vec());
            let handle = self.get_next_handle();
            self.objects.insert(handle, object);
        }
        Ok(())
    }

    pub fn open_session(&mut self, slot_type: SlotType) -> Result<CK_SESSION_HANDLE, Error> {
        let next_session = self.next_session;
        self.next_session += 1;
        self.sessions.insert(next_session, slot_type);
        Ok(next_session)
    }

    pub fn close_session(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        self.sessions
            .remove(&session)
            .ok_or_else(|| error_here!(ErrorType::InvalidInput))
            .map(|_| ())
    }

    pub fn close_all_sessions(&mut self, slot_type: SlotType) -> Result<(), Error> {
        let mut to_remove = Vec::new();
        for (session, open_slot_type) in self.sessions.iter() {
            if slot_type == *open_slot_type {
                to_remove.push(*session);
            }
        }
        for session in to_remove {
            if self.sessions.remove(&session).is_none() {
                return Err(error_here!(ErrorType::LibraryFailure));
            }
        }
        Ok(())
    }

    fn get_next_handle(&mut self) -> CK_OBJECT_HANDLE {
        let next_handle = self.next_handle;
        self.next_handle += 1;
        next_handle
    }

    /// PKCS #11 specifies that search operations happen in three phases: setup, get any matches
    /// (this part may be repeated if the caller uses a small buffer), and end. This implementation
    /// does all of the work up front and gathers all matching objects during setup and retains them
    /// until they are retrieved and consumed via `search`.
    pub fn start_search(
        &mut self,
        session: CK_SESSION_HANDLE,
        attrs: Vec<(CK_ATTRIBUTE_TYPE, Vec<u8>)>,
    ) -> Result<(), Error> {
        let slot_type = match self.sessions.get(&session) {
            Some(slot_type) => *slot_type,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        // If the search is for an attribute we don't support, no objects will match. This check
        // saves us having to look through all of our objects.
        for (attr, _) in &attrs {
            if !SUPPORTED_ATTRIBUTES.contains(attr) {
                self.searches.insert(session, Vec::new());
                return Ok(());
            }
        }
        // When NSS wants to find all certificates or all private keys, it will perform a search
        // with a particular set of attributes. This implementation uses these searches as an
        // indication for the backend to re-scan for new objects from tokens that may have been
        // inserted or certificates that may have been imported into the OS. Since these searches
        // are relatively rare, this minimizes the impact of doing these re-scans.
        if search_is_for_all_certificates_or_keys(&attrs)? {
            self.maybe_find_new_objects()?;
        }
        let mut handles = Vec::new();
        for (handle, object) in &self.objects {
            if object.matches(slot_type, &attrs) {
                handles.push(*handle);
            }
        }
        self.searches.insert(session, handles);
        Ok(())
    }

    /// Given a session and a maximum number of object handles to return, attempts to retrieve up to
    /// that many objects from the corresponding search. Updates the search so those objects are not
    /// returned repeatedly. `max_objects` must be non-zero.
    pub fn search(
        &mut self,
        session: CK_SESSION_HANDLE,
        max_objects: usize,
    ) -> Result<Vec<CK_OBJECT_HANDLE>, Error> {
        if max_objects == 0 {
            return Err(error_here!(ErrorType::InvalidArgument));
        }
        match self.searches.get_mut(&session) {
            Some(search) => {
                let split_at = if max_objects >= search.len() {
                    0
                } else {
                    search.len() - max_objects
                };
                let to_return = search.split_off(split_at);
                if to_return.len() > max_objects {
                    return Err(error_here!(ErrorType::LibraryFailure));
                }
                Ok(to_return)
            }
            None => Err(error_here!(ErrorType::InvalidArgument)),
        }
    }

    pub fn clear_search(&mut self, session: CK_SESSION_HANDLE) -> Result<(), Error> {
        self.searches.remove(&session);
        Ok(())
    }

    pub fn get_attributes(
        &self,
        object_handle: CK_OBJECT_HANDLE,
        attr_types: Vec<CK_ATTRIBUTE_TYPE>,
    ) -> Result<Vec<Option<Vec<u8>>>, Error> {
        let object = match self.objects.get(&object_handle) {
            Some(object) => object,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let mut results = Vec::with_capacity(attr_types.len());
        for attr_type in attr_types {
            let result = object
                .get_attribute(attr_type)
                .map(|value| value.to_owned());
            results.push(result);
        }
        Ok(results)
    }

    /// The way NSS uses PKCS #11 to sign data happens in two phases: setup and sign. This
    /// implementation makes a note of which key is to be used (if it exists) during setup. When the
    /// caller finishes with the sign operation, this implementation retrieves the key handle and
    /// performs the signature.
    pub fn start_sign(
        &mut self,
        session: CK_SESSION_HANDLE,
        key_handle: CK_OBJECT_HANDLE,
        params: Option<CK_RSA_PKCS_PSS_PARAMS>,
    ) -> Result<(), Error> {
        if self.signs.contains_key(&session) {
            return Err(error_here!(ErrorType::InvalidArgument));
        }
        self.signs.insert(session, (key_handle, params));
        Ok(())
    }

    pub fn get_signature_length(
        &mut self,
        session: CK_SESSION_HANDLE,
        data: Vec<u8>,
    ) -> Result<usize, Error> {
        let (key_handle, params) = match self.signs.get(&session) {
            Some((key_handle, params)) => (key_handle, params),
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let key = match self.objects.get_mut(key_handle) {
            Some(key) => key,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        key.get_signature_length(data, params)
    }

    pub fn sign(&mut self, session: CK_SESSION_HANDLE, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        // Performing the signature (via C_Sign, which is the only way we support) finishes the sign
        // operation, so it needs to be removed here.
        let (key_handle, params) = match self.signs.remove(&session) {
            Some((key_handle, params)) => (key_handle, params),
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        let key = match self.objects.get_mut(&key_handle) {
            Some(key) => key,
            None => return Err(error_here!(ErrorType::InvalidArgument)),
        };
        key.sign(data, &params)
    }
}
