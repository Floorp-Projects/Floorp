/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate crossbeam_utils;
extern crate lmdb;
extern crate moz_task;
extern crate nserror;
extern crate nsstring;
extern crate rkv;
extern crate sha2;
extern crate thin_vec;
extern crate time;
#[macro_use]
extern crate xpcom;
extern crate style;

use crossbeam_utils::atomic::AtomicCell;
use lmdb::EnvironmentFlags;
use moz_task::{create_thread, is_main_thread, Task, TaskRunnable};
use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_NOT_SAME_THREAD, NS_ERROR_NO_AGGREGATION,
    NS_ERROR_UNEXPECTED, NS_OK,
};
use nsstring::{nsACString, nsAString, nsCStr, nsCString, nsString};
use rkv::{Rkv, SingleStore, StoreOptions, Value};
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::fmt::Display;
use std::fs::{create_dir_all, remove_file, File};
use std::io::{BufRead, BufReader};
use std::os::raw::c_char;
use std::path::PathBuf;
use std::slice;
use std::str;
use std::sync::{Arc, Mutex, RwLock};
use std::time::{Duration, SystemTime};
use thin_vec::ThinVec;
use xpcom::interfaces::{
    nsICertStorage, nsICertStorageCallback, nsIFile, nsIObserver, nsIPrefBranch, nsISupports,
    nsIThread,
};
use xpcom::{nsIID, GetterAddrefs, RefPtr, ThreadBoundRefPtr, XpCom};

const PREFIX_REV_IS: &str = "is";
const PREFIX_REV_SPK: &str = "spk";
const PREFIX_CRLITE: &str = "crlite";
const PREFIX_WL: &str = "wl";

fn make_key(prefix: &str, part_a: &[u8], part_b: &[u8]) -> Vec<u8> {
    let mut key = prefix.as_bytes().to_owned();
    key.extend_from_slice(part_a);
    key.extend_from_slice(part_b);
    key
}

#[allow(non_camel_case_types, non_snake_case)]

/// `SecurityStateError` is a type to represent errors in accessing or
/// modifying security state.
#[derive(Debug)]
struct SecurityStateError {
    message: String,
}

impl<T: Display> From<T> for SecurityStateError {
    /// Creates a new instance of `SecurityStateError` from something that
    /// implements the `Display` trait.
    fn from(err: T) -> SecurityStateError {
        SecurityStateError {
            message: format!("{}", err),
        }
    }
}

struct EnvAndStore {
    env: Rkv,
    store: SingleStore,
}

/// `SecurityState`
struct SecurityState {
    profile_path: PathBuf,
    env_and_store: Option<EnvAndStore>,
    int_prefs: HashMap<String, i32>,
}

impl SecurityState {
    pub fn new(profile_path: PathBuf) -> Result<SecurityState, SecurityStateError> {
        // Since this gets called on the main thread, we don't actually want to open the DB yet.
        // We do this on-demand later, when we're probably on a certificate verification thread.
        Ok(SecurityState {
            profile_path,
            env_and_store: None,
            int_prefs: HashMap::new(),
        })
    }

    pub fn db_needs_opening(&self) -> bool {
        self.env_and_store.is_none()
    }

    pub fn open_db(&mut self) -> Result<(), SecurityStateError> {
        if self.env_and_store.is_some() {
            return Ok(());
        }

        let store_path = get_store_path(&self.profile_path)?;

        // Open the store in read-write mode initially to create it (if needed)
        // and migrate data from the old store (if any).
        let env = Rkv::new(store_path.as_path())?;
        let store = env.open_single("cert_storage", StoreOptions::create())?;

        // if the profile has a revocations.txt, migrate it and remove the file
        let mut revocations_path = self.profile_path.clone();
        revocations_path.push("revocations.txt");
        if revocations_path.exists() {
            SecurityState::migrate(&revocations_path, &env, &store)?;
            remove_file(revocations_path)?;
        }

        // We already returned early if env_and_store was Some, so this should take the None branch.
        match self.env_and_store.replace(EnvAndStore { env, store }) {
            Some(_) => Err(SecurityStateError::from(
                "env and store already initialized? (did we mess up our threading model?)",
            )),
            None => Ok(()),
        }?;

        // Now reopen the store in read-only mode to conserve memory.
        // We'll reopen it again in read-write mode when making changes.
        self.reopen_store_read_only()
    }

    fn migrate(
        revocations_path: &PathBuf,
        env: &Rkv,
        store: &SingleStore,
    ) -> Result<(), SecurityStateError> {
        let f = File::open(revocations_path)?;
        let file = BufReader::new(f);
        let value = Value::I64(nsICertStorage::STATE_ENFORCE as i64);
        let mut writer = env.write()?;

        // Add the data from revocations.txt
        let mut dn: Option<Vec<u8>> = None;
        for line in file.lines() {
            let l = match line.map_err(|_| SecurityStateError::from("io error reading line data")) {
                Ok(data) => data,
                Err(e) => return Err(e),
            };
            if l.len() == 0 || l.starts_with("#") {
                continue;
            }
            let leading_char = match l.chars().next() {
                Some(c) => c,
                None => {
                    return Err(SecurityStateError::from(
                        "couldn't get char from non-empty str?",
                    ));
                }
            };
            // In future, we can maybe log migration failures. For now, ignore decoding and storage
            // errors and attempt to continue.
            // Check if we have a new DN
            if leading_char != '\t' && leading_char != ' ' {
                if let Ok(decoded_dn) = base64::decode(&l) {
                    dn = Some(decoded_dn);
                }
                continue;
            }
            let l_sans_prefix = match base64::decode(&l[1..]) {
                Ok(decoded) => decoded,
                Err(_) => continue,
            };
            if let Some(name) = &dn {
                if leading_char == '\t' {
                    let _ = store.put(
                        &mut writer,
                        &make_key(PREFIX_REV_SPK, name, &l_sans_prefix),
                        &value,
                    );
                } else {
                    let _ = store.put(
                        &mut writer,
                        &make_key(PREFIX_REV_IS, name, &l_sans_prefix),
                        &value,
                    );
                }
            }
        }

        writer.commit()?;
        Ok(())
    }

    fn reopen_store_read_write(&mut self) -> Result<(), SecurityStateError> {
        let store_path = get_store_path(&self.profile_path)?;

        // Move out and drop the EnvAndStore first so we don't have
        // two LMDB environments open at the same time.
        drop(self.env_and_store.take());

        let env = Rkv::new(store_path.as_path())?;
        let store = env.open_single("cert_storage", StoreOptions::create())?;
        self.env_and_store.replace(EnvAndStore { env, store });
        Ok(())
    }

    fn reopen_store_read_only(&mut self) -> Result<(), SecurityStateError> {
        let store_path = get_store_path(&self.profile_path)?;

        // Move out and drop the EnvAndStore first so we don't have
        // two LMDB environments open at the same time.
        drop(self.env_and_store.take());

        let mut builder = Rkv::environment_builder();
        builder.set_max_dbs(2);
        builder.set_flags(EnvironmentFlags::READ_ONLY);

        let env = Rkv::from_env(store_path.as_path(), builder)?;
        let store = env.open_single("cert_storage", StoreOptions::default())?;
        self.env_and_store.replace(EnvAndStore { env, store });
        Ok(())
    }

    fn write_entry(&mut self, key: &[u8], value: i16) -> Result<(), SecurityStateError> {
        self.reopen_store_read_write()?;
        {
            let env_and_store = match self.env_and_store.as_mut() {
                Some(env_and_store) => env_and_store,
                None => return Err(SecurityStateError::from("env and store not initialized?")),
            };
            let mut writer = env_and_store.env.write()?;
            env_and_store
                .store
                .put(&mut writer, key, &Value::I64(value as i64))?;
            writer.commit()?;
        }
        self.reopen_store_read_only()?;
        Ok(())
    }

    fn read_entry(&self, key: &[u8]) -> Result<Option<i16>, SecurityStateError> {
        let env_and_store = match self.env_and_store.as_ref() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let reader = env_and_store.env.read()?;
        match env_and_store.store.get(&reader, key) {
            Ok(Some(Value::I64(i)))
                if i <= (std::i16::MAX as i64) && i >= (std::i16::MIN as i64) =>
            {
                Ok(Some(i as i16))
            }
            Ok(None) => Ok(None),
            _ => Err(SecurityStateError::from(
                "There was a problem getting the value",
            )),
        }
    }

    pub fn set_revocation_by_issuer_and_serial(
        &mut self,
        issuer: &[u8],
        serial: &[u8],
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&make_key(PREFIX_REV_IS, issuer, serial), state)
    }

    pub fn set_revocation_by_subject_and_pub_key(
        &mut self,
        subject: &[u8],
        pub_key_hash: &[u8],
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&make_key(PREFIX_REV_SPK, subject, pub_key_hash), state)
    }

    pub fn set_enrollment(
        &mut self,
        issuer: &[u8],
        serial: &[u8],
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&make_key(PREFIX_CRLITE, issuer, serial), state)
    }

    pub fn set_whitelist(
        &mut self,
        issuer: &[u8],
        serial: &[u8],
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&make_key(PREFIX_WL, issuer, serial), state)
    }

    pub fn get_revocation_state(
        &self,
        issuer: &[u8],
        serial: &[u8],
        subject: &[u8],
        pub_key: &[u8],
    ) -> Result<i16, SecurityStateError> {
        let mut digest = Sha256::default();
        digest.input(pub_key);
        let pub_key_hash = digest.result();

        let subject_pubkey = make_key(PREFIX_REV_SPK, subject, &pub_key_hash);
        let issuer_serial = make_key(PREFIX_REV_IS, issuer, serial);

        let st: i16 = match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => value,
            Ok(None) => nsICertStorage::STATE_UNSET as i16,
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem reading revocation state (from issuer / serial)",
                ));
            }
        };

        if st != nsICertStorage::STATE_UNSET as i16 {
            return Ok(st);
        }

        match self.read_entry(&subject_pubkey) {
            Ok(Some(value)) => Ok(value),
            Ok(None) => Ok(nsICertStorage::STATE_UNSET as i16),
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem reading revocation state (from subject / pubkey)",
                ));
            }
        }
    }

    pub fn get_enrollment_state(
        &self,
        issuer: &[u8],
        serial: &[u8],
    ) -> Result<i16, SecurityStateError> {
        let issuer_serial = make_key(PREFIX_CRLITE, issuer, serial);
        match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => Ok(value),
            Ok(None) => Ok(nsICertStorage::STATE_UNSET as i16),
            Err(_) => return Err(SecurityStateError::from("problem reading enrollment state")),
        }
    }

    pub fn get_whitelist_state(
        &self,
        issuer: &[u8],
        serial: &[u8],
    ) -> Result<i16, SecurityStateError> {
        let issuer_serial = make_key(PREFIX_WL, issuer, serial);
        match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => Ok(value),
            Ok(None) => Ok(nsICertStorage::STATE_UNSET as i16),
            Err(_) => Err(SecurityStateError::from("problem reading whitelist state")),
        }
    }

    pub fn is_data_fresh(
        &self,
        update_pref: &str,
        allowed_staleness: &str,
    ) -> Result<bool, SecurityStateError> {
        let checked = match self.int_prefs.get(update_pref) {
            Some(ch) => *ch,
            None => 0,
        };
        let staleness_seconds = match self.int_prefs.get(allowed_staleness) {
            Some(st) => *st,
            None => 0,
        };

        let update = SystemTime::UNIX_EPOCH + Duration::new(checked as u64, 0);
        let staleness = Duration::new(staleness_seconds as u64, 0);

        Ok(match SystemTime::now().duration_since(update) {
            Ok(duration) => duration <= staleness,
            Err(_) => false,
        })
    }

    pub fn is_blocklist_fresh(&self) -> Result<bool, SecurityStateError> {
        self.is_data_fresh(
            "services.blocklist.onecrl.checked",
            "security.onecrl.maximum_staleness_in_seconds",
        )
    }

    pub fn is_whitelist_fresh(&self) -> Result<bool, SecurityStateError> {
        self.is_data_fresh(
            "services.blocklist.intermediates.checked",
            "security.onecrl.maximum_staleness_in_seconds",
        )
    }

    pub fn is_enrollment_fresh(&self) -> Result<bool, SecurityStateError> {
        self.is_data_fresh(
            "services.blocklist.crlite.checked",
            "security.onecrl.maximum_staleness_in_seconds",
        )
    }

    pub fn pref_seen(&mut self, name: &str, value: i32) {
        self.int_prefs.insert(name.to_owned(), value);
    }
}

fn get_path_from_directory_service(key: &str) -> Result<PathBuf, SecurityStateError> {
    let directory_service = match xpcom::services::get_DirectoryService() {
        Some(ds) => ds,
        _ => return Err(SecurityStateError::from("None")),
    };

    let cs_key = CString::new(key)?;
    let mut requested_dir = GetterAddrefs::<nsIFile>::new();

    unsafe {
        (*directory_service)
            .Get(
                (&cs_key).as_ptr(),
                &nsIFile::IID as *const nsIID,
                requested_dir.void_ptr(),
            )
            .to_result()
            .map_err(|res| SecurityStateError {
                message: (*res.error_name()).as_str_unchecked().to_owned(),
            })
    }?;

    let dir_path = match requested_dir.refptr() {
        None => return Err(SecurityStateError::from("directory service failure")),
        Some(refptr) => refptr,
    };

    let mut path = nsString::new();

    unsafe {
        (*dir_path)
            .GetPath(&mut path as &mut nsAString)
            // For reasons that aren't clear to me, NsresultExt does not
            // implement std::error::Error (or Debug / Display). This map_err
            // hack is a way to get an error with a useful message.
            .to_result()
            .map_err(|res| SecurityStateError {
                message: (*res.error_name()).as_str_unchecked().to_owned(),
            })?;
    }

    Ok(PathBuf::from(format!("{}", path)))
}

fn get_profile_path() -> Result<PathBuf, SecurityStateError> {
    Ok(get_path_from_directory_service("ProfD").or_else(|_| get_path_from_directory_service("TmpD"))?)
}

fn get_store_path(profile_path: &PathBuf) -> Result<PathBuf, SecurityStateError> {
    let mut store_path = profile_path.clone();
    store_path.push("security_state");
    create_dir_all(store_path.as_path())?;
    Ok(store_path)
}

fn do_construct_cert_storage(
    _outer: *const nsISupports,
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> Result<(), SecurityStateError> {
    let path_buf = get_profile_path()?;

    let cert_storage = CertStorage::allocate(InitCertStorage {
        security_state: Arc::new(RwLock::new(SecurityState::new(path_buf)?)),
        thread: Mutex::new(create_thread("cert_storage")?),
    });

    unsafe {
        cert_storage
            .QueryInterface(iid, result)
            // As above; greasy hack because NsresultExt
            .to_result()
            .map_err(|res| SecurityStateError {
                message: (*res.error_name()).as_str_unchecked().to_owned(),
            })?;

        return cert_storage.setup_prefs();
    };
}

fn read_int_pref(name: &str) -> Result<i32, SecurityStateError> {
    let pref_service = match xpcom::services::get_PreferencesService() {
        Some(ps) => ps,
        _ => {
            return Err(SecurityStateError::from(
                "could not get preferences service",
            ));
        }
    };

    let prefs: RefPtr<nsIPrefBranch> = match (*pref_service).query_interface() {
        Some(pb) => pb,
        _ => return Err(SecurityStateError::from("could not QI to nsIPrefBranch")),
    };
    let pref_name = match CString::new(name) {
        Ok(n) => n,
        _ => return Err(SecurityStateError::from("could not build pref name string")),
    };

    let mut pref_value: i32 = -1;

    // We can't use GetIntPrefWithDefault because optional_argc is not
    // supported. No matter, we can just check for failure and ignore
    // any NS_ERROR_UNEXPECTED result.
    let res = unsafe { (*prefs).GetIntPref((&pref_name).as_ptr(), (&mut pref_value) as *mut i32) };
    match res {
        NS_OK => Ok(pref_value),
        NS_ERROR_UNEXPECTED => Ok(pref_value),
        _ => Err(SecurityStateError::from("could not read pref")),
    }
}

// This is a helper for defining a task that will perform a specific action on a background thread.
// Its arguments are the name of the task and the name of the function in SecurityState to call.
macro_rules! security_state_task {
    ($task_name:ident, $security_state_function_name:ident) => {
        struct $task_name {
            callback: AtomicCell<Option<ThreadBoundRefPtr<nsICertStorageCallback>>>,
            security_state: Arc<RwLock<SecurityState>>,
            argument_a: Vec<u8>,
            argument_b: Vec<u8>,
            state: i16,
            result: AtomicCell<Option<nserror::nsresult>>,
        }
        impl $task_name {
            fn new(
                callback: &nsICertStorageCallback,
                security_state: &Arc<RwLock<SecurityState>>,
                argument_a: Vec<u8>,
                argument_b: Vec<u8>,
                state: i16,
            ) -> $task_name {
                $task_name {
                    callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(RefPtr::new(callback)))),
                    security_state: Arc::clone(security_state),
                    argument_a,
                    argument_b,
                    state,
                    result: AtomicCell::new(None),
                }
            }
        }
        impl Task for $task_name {
            fn run(&self) {
                let mut ss = match self.security_state.write() {
                    Ok(ss) => ss,
                    Err(_) => {
                        self.result.store(Some(NS_ERROR_FAILURE));
                        return;
                    }
                };
                // this is a no-op if the DB is already open
                match ss.open_db() {
                    Ok(()) => {}
                    Err(_) => {
                        self.result.store(Some(NS_ERROR_FAILURE));
                        return;
                    }
                };
                match ss.$security_state_function_name(
                    &self.argument_a,
                    &self.argument_b,
                    self.state,
                ) {
                    Ok(_) => self.result.store(Some(NS_OK)),
                    Err(_) => self.result.store(Some(NS_ERROR_FAILURE)),
                };
            }

            fn done(&self) -> Result<(), nsresult> {
                let threadbound = self.callback.swap(None).ok_or(NS_ERROR_FAILURE)?;
                let callback = threadbound.get_ref().ok_or(NS_ERROR_FAILURE)?;
                let nsrv = match self.result.swap(None) {
                    Some(result) => unsafe { callback.Done(result) },
                    None => unsafe { callback.Done(NS_ERROR_FAILURE) },
                };
                match nsrv {
                    NS_OK => Ok(()),
                    e => Err(e),
                }
            }
        }
    };
}

security_state_task!(
    SetRevocationByIssuerAndSerialTask,
    set_revocation_by_issuer_and_serial
);
security_state_task!(
    SetRevocationBySubjectAndPubKeyTask,
    set_revocation_by_subject_and_pub_key
);
security_state_task!(SetEnrollmentTask, set_enrollment);
security_state_task!(SetWhitelistTask, set_whitelist);

#[no_mangle]
pub extern "C" fn cert_storage_constructor(
    outer: *const nsISupports,
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nserror::nsresult {
    if !outer.is_null() {
        return NS_ERROR_NO_AGGREGATION;
    }

    match do_construct_cert_storage(outer, iid, result) {
        Ok(_) => NS_OK,
        Err(_) => {
            // In future: log something so we know what went wrong?
            NS_ERROR_FAILURE
        }
    }
}

macro_rules! try_ns {
    ($e:expr) => {
        match $e {
            Ok(value) => value,
            Err(_) => return NS_ERROR_FAILURE,
        }
    };
}

#[derive(xpcom)]
#[xpimplements(nsICertStorage, nsIObserver)]
#[refcnt = "atomic"]
struct InitCertStorage {
    security_state: Arc<RwLock<SecurityState>>,
    thread: Mutex<RefPtr<nsIThread>>,
}

/// CertStorage implements the nsICertStorage interface. The actual work is done by the
/// SecurityState. To handle any threading issues, we have an atomic-refcounted read/write lock on
/// the one and only SecurityState. So, only one thread can use SecurityState's &mut self functions
/// at a time, while multiple threads can use &self functions simultaneously (as long as there are
/// no threads using an &mut self function). The Arc is to allow for the creation of background
/// tasks that use the SecurityState on the thread owned by CertStorage. This allows us to not block
/// the main thread.
#[allow(non_snake_case)]
impl CertStorage {
    unsafe fn setup_prefs(&self) -> Result<(), SecurityStateError> {
        let int_prefs = [
            "services.blocklist.onecrl.checked",
            "services.blocklist.intermediates.checked",
            "services.blocklist.crlite.checked",
            "security.onecrl.maximum_staleness_in_seconds",
        ];

        // Fetch add observers for relevant prefs
        let pref_service = xpcom::services::get_PreferencesService().unwrap();
        let prefs: RefPtr<nsIPrefBranch> = match (*pref_service).query_interface() {
            Some(pb) => pb,
            _ => return Err(SecurityStateError::from("could not QI to nsIPrefBranch")),
        };

        for pref in int_prefs.into_iter() {
            let pref_nscstr = &nsCStr::from(pref.to_owned()) as &nsACString;
            let rv = (*prefs).AddObserverImpl(pref_nscstr, self.coerce::<nsIObserver>(), false);
            match read_int_pref(pref) {
                Ok(up) => {
                    let mut ss = self.security_state.write()?;
                    // This doesn't use the DB, so no need to open it first. (Also since we do this
                    // upon initialization, it would defeat the purpose of lazily opening the DB.)
                    ss.pref_seen(pref, up);
                }
                Err(_) => return Err(SecurityStateError::from("could not read pref")),
            };
            assert!(rv.succeeded());
        }

        Ok(())
    }

    unsafe fn SetRevocationByIssuerAndSerial(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: i16,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if issuer.is_null() || serial.is_null() || callback.is_null() {
            return NS_ERROR_FAILURE;
        }
        let issuer_decoded = try_ns!(base64::decode(&*issuer));
        let serial_decoded = try_ns!(base64::decode(&*serial));
        let task = Box::new(SetRevocationByIssuerAndSerialTask::new(
            &*callback,
            &self.security_state,
            issuer_decoded,
            serial_decoded,
            state,
        ));
        let thread = try_ns!(self.thread.lock());
        let runnable = try_ns!(TaskRunnable::new("SetRevocationByIssuerAndSerial", task));
        try_ns!(runnable.dispatch(&*thread));
        NS_OK
    }

    unsafe fn SetRevocationBySubjectAndPubKey(
        &self,
        subject: *const nsACString,
        pub_key_hash: *const nsACString,
        state: i16,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if subject.is_null() || pub_key_hash.is_null() || callback.is_null() {
            return NS_ERROR_FAILURE;
        }
        let subject_decoded = try_ns!(base64::decode(&*subject));
        let pub_key_hash_decoded = try_ns!(base64::decode(&*pub_key_hash));
        let task = Box::new(SetRevocationBySubjectAndPubKeyTask::new(
            &*callback,
            &self.security_state,
            subject_decoded,
            pub_key_hash_decoded,
            state,
        ));
        let thread = try_ns!(self.thread.lock());
        let runnable = try_ns!(TaskRunnable::new("SetRevocationBySubjectAndPubKey", task));
        try_ns!(runnable.dispatch(&*thread));
        NS_OK
   }

    unsafe fn SetEnrollment(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: i16,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if issuer.is_null() || serial.is_null() || callback.is_null() {
            return NS_ERROR_FAILURE;
        }
        let issuer_decoded = try_ns!(base64::decode(&*issuer));
        let serial_decoded = try_ns!(base64::decode(&*serial));
        let task = Box::new(SetEnrollmentTask::new(
            &*callback,
            &self.security_state,
            issuer_decoded,
            serial_decoded,
            state,
        ));
        let thread = try_ns!(self.thread.lock());
        let runnable = try_ns!(TaskRunnable::new("SetEnrollment", task));
        try_ns!(runnable.dispatch(&*thread));
        NS_OK
    }

    unsafe fn SetWhitelist(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: i16,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if issuer.is_null() || serial.is_null() || callback.is_null() {
            return NS_ERROR_FAILURE;
        }
        let issuer_decoded = try_ns!(base64::decode(&*issuer));
        let serial_decoded = try_ns!(base64::decode(&*serial));
        let task = Box::new(SetWhitelistTask::new(
            &*callback,
            &self.security_state,
            issuer_decoded,
            serial_decoded,
            state,
        ));
        let thread = try_ns!(self.thread.lock());
        let runnable = try_ns!(TaskRunnable::new("SetWhitelist", task));
        try_ns!(runnable.dispatch(&*thread));
        NS_OK
    }

    unsafe fn GetRevocationState(
        &self,
        issuer: *const ThinVec<u8>,
        serial: *const ThinVec<u8>,
        subject: *const ThinVec<u8>,
        pub_key: *const ThinVec<u8>,
        state: *mut i16,
    ) -> nserror::nsresult {
        // TODO (bug 1541212): We really want to restrict this to non-main-threads only, but we
        // can't do so until bug 1406854 and bug 1534600 are fixed.
        if issuer.is_null() || serial.is_null() || subject.is_null() || pub_key.is_null() {
            return NS_ERROR_FAILURE;
        }
        *state = nsICertStorage::STATE_UNSET as i16;
        // The following is a way to ensure the DB has been opened while minimizing lock
        // acquisitions in the common (read-only) case. First we acquire a read lock and see if we
        // even need to open the DB. If not, we can continue with the read lock we already have.
        // Otherwise, we drop the read lock, acquire the write lock, open the DB, drop the write
        // lock, and re-acquire the read lock. While it is possible for two or more threads to all
        // come to the conclusion that they need to open the DB, this isn't ultimately an issue -
        // `open_db` will exit early if another thread has already done the work.
        let ss = {
            let ss_read_only = try_ns!(self.security_state.read());
            if !ss_read_only.db_needs_opening() {
                ss_read_only
            } else {
                drop(ss_read_only);
                {
                    let mut ss_write = try_ns!(self.security_state.write());
                    try_ns!(ss_write.open_db());
                }
                try_ns!(self.security_state.read())
            }
        };
        match ss.get_revocation_state(&*issuer, &*serial, &*subject, &*pub_key) {
            Ok(st) => {
                *state = st;
                NS_OK
            }
            _ => NS_ERROR_FAILURE,
        }
    }

    unsafe fn GetEnrollmentState(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: *mut i16,
    ) -> nserror::nsresult {
        if is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if issuer.is_null() || serial.is_null() {
            return NS_ERROR_FAILURE;
        }
        let issuer_decoded = try_ns!(base64::decode(&*issuer));
        let serial_decoded = try_ns!(base64::decode(&*serial));
        *state = nsICertStorage::STATE_UNSET as i16;
        let ss = {
            let ss_read_only = try_ns!(self.security_state.read());
            if !ss_read_only.db_needs_opening() {
                ss_read_only
            } else {
                drop(ss_read_only);
                {
                    let mut ss_write = try_ns!(self.security_state.write());
                    try_ns!(ss_write.open_db());
                }
                try_ns!(self.security_state.read())
            }
        };
        match ss.get_enrollment_state(&issuer_decoded, &serial_decoded) {
            Ok(st) => {
                *state = st;
                NS_OK
            }
            _ => NS_ERROR_FAILURE,
        }
    }

    unsafe fn GetWhitelistState(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: *mut i16,
    ) -> nserror::nsresult {
        if is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if issuer.is_null() || serial.is_null() {
            return NS_ERROR_FAILURE;
        }
        let issuer_decoded = try_ns!(base64::decode(&*issuer));
        let serial_decoded = try_ns!(base64::decode(&*serial));
        *state = nsICertStorage::STATE_UNSET as i16;
        let ss = {
            let ss_read_only = try_ns!(self.security_state.read());
            if !ss_read_only.db_needs_opening() {
                ss_read_only
            } else {
                drop(ss_read_only);
                {
                    let mut ss_write = try_ns!(self.security_state.write());
                    try_ns!(ss_write.open_db());
                }
                try_ns!(self.security_state.read())
            }
        };
        match ss.get_whitelist_state(&issuer_decoded, &serial_decoded) {
            Ok(st) => {
                *state = st;
                NS_OK
            }
            _ => NS_ERROR_FAILURE,
        }
    }

    unsafe fn IsBlocklistFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;
        let ss = try_ns!(self.security_state.read());
        // This doesn't use the db -> don't need to make sure it's open.
        *fresh = match ss.is_blocklist_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        NS_OK
    }

    unsafe fn IsWhitelistFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;
        let ss = try_ns!(self.security_state.read());
        // This doesn't use the db -> don't need to make sure it's open.
        *fresh = match ss.is_whitelist_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        NS_OK
    }

    unsafe fn IsEnrollmentFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;
        let ss = try_ns!(self.security_state.read());
        // This doesn't use the db -> don't need to make sure it's open.
        *fresh = match ss.is_enrollment_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        NS_OK
    }

    unsafe fn Observe(
        &self,
        subject: *const nsISupports,
        topic: *const c_char,
        pref_name: *const i16,
    ) -> nserror::nsresult {
        match CStr::from_ptr(topic).to_str() {
            Ok("nsPref:changed") => {
                let mut pref_value: i32 = 0;

                let prefs: RefPtr<nsIPrefBranch> = match (*subject).query_interface() {
                    Some(pb) => pb,
                    _ => return NS_ERROR_FAILURE,
                };

                // Convert our wstring pref_name to a cstring (via nsCString's
                // utf16 to utf8 conversion)
                let mut len: usize = 0;
                while (*(pref_name.offset(len as isize))) != 0 {
                    len += 1;
                }
                let name_slice = slice::from_raw_parts(pref_name as *const u16, len);
                let mut name_string = nsCString::new();
                name_string.assign_utf16_to_utf8(name_slice);

                let pref_name = match CString::new(name_string.as_str_unchecked()) {
                    Ok(n) => n,
                    _ => return NS_ERROR_FAILURE,
                };

                let res = prefs.GetIntPref((&pref_name).as_ptr(), (&mut pref_value) as *mut i32);

                if !res.succeeded() {
                    return res;
                }

                let mut ss = try_ns!(self.security_state.write());
                // This doesn't use the db -> don't need to make sure it's open.
                ss.pref_seen(name_string.as_str_unchecked(), pref_value);
            }
            _ => (),
        }
        NS_OK
    }
}
