/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate nserror;
extern crate nsstring;
extern crate rkv;
extern crate sha2;
extern crate time;
#[macro_use]
extern crate xpcom;
extern crate style;

use nsstring::{nsACString, nsAString, nsCStr, nsCString, nsString};
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
use std::sync::RwLock;
use std::time::{Duration, SystemTime};
use style::gecko_bindings::structs::nsresult;
use xpcom::interfaces::{nsICertStorage, nsIFile, nsIObserver, nsIPrefBranch, nsISupports};
use xpcom::{nsIID, GetterAddrefs, RefPtr, XpCom};

use rkv::{Rkv, StoreOptions, Value};

const PREFIX_REV_IS: &str = "is";
const PREFIX_REV_SPK: &str = "spk";
const PREFIX_CRLITE: &str = "crlite";
const PREFIX_WL: &str = "wl";

#[allow(non_camel_case_types, non_snake_case)]

/// `SecurityStateError` is a type to represent errors in accessing or
/// modifying security state.
#[derive(Debug)]
pub struct SecurityStateError {
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

/// `SecurityState`
pub struct SecurityState {
    path: PathBuf,
    int_prefs: HashMap<String, i32>,
}

impl SecurityState {
    pub fn new(profile_path: PathBuf) -> Result<SecurityState, SecurityStateError> {
        let mut store_path = profile_path.clone();
        store_path.push("security_state");

        create_dir_all(store_path.as_path())?;
        let mut ss = SecurityState {
            path: store_path,
            int_prefs: HashMap::new(),
        };

        let mut revocations_path = profile_path;
        revocations_path.push("revocations.txt");

        // if the profile has a revocations.txt, migrate it and remove the file
        if revocations_path.exists() {
            ss.migrate(&revocations_path)?;
            remove_file(revocations_path)?;
        }
        Ok(ss)
    }

    fn migrate(&mut self, revocations_path: &PathBuf) -> Result<(), SecurityStateError> {
        let f = File::open(revocations_path)?;
        let file = BufReader::new(f);

        // Add the data from revocations.txt
        let mut dn: Option<String> = None;
        let mut l;
        for line in file.lines() {
            l = match line.map_err(|_| SecurityStateError::from("io error reading line data")) {
                Ok(data) => data.to_owned(),
                Err(e) => return Err(e),
            };
            let l_sans_prefix = match l.len() {
                0 => "".to_owned(),
                _ => l[1..].to_owned(),
            };
            // In future, we can maybe log migration failures. For now, ignore the error
            // and attempt to continue.
            let _ = match l.chars().next() {
                Some('#') => Ok(()),
                Some('\t') => match &dn {
                    Some(name) => self.set_revocation_by_subject_and_pub_key(
                        &name,
                        &l_sans_prefix,
                        nsICertStorage::STATE_ENFORCE as i16,
                    ),
                    None => Err(SecurityStateError::from("pubkey with no subject")),
                },
                Some(' ') => match &dn {
                    Some(name) => self.set_revocation_by_issuer_and_serial(
                        &name,
                        &l_sans_prefix,
                        nsICertStorage::STATE_ENFORCE as i16,
                    ),
                    None => Err(SecurityStateError::from("serial with no issuer")),
                },
                Some(_) => {
                    dn = Some(l);
                    Ok(())
                }
                None => Ok(()),
            };
        }

        Ok(())
    }

    fn write_entry(&mut self, key: &str, value: i16) -> Result<(), SecurityStateError> {
        let p = self.path.as_path();
        let env = Rkv::new(p)?;
        let store = env.open_single("cert_storage", StoreOptions::create())?;

        let mut writer = env.write()?;
        store.put(&mut writer, key, &Value::I64(value as i64))?;
        writer.commit()?;
        Ok(())
    }

    fn read_entry(&self, key: &str) -> Result<Option<i16>, SecurityStateError> {
        // TODO: figure out a way to de-dupe getting the env / store
        let p = self.path.as_path();
        let env = Rkv::new(p)?;
        let store = env.open_single("cert_storage", StoreOptions::create())?;

        let reader = env.read()?;
        match store.get(&reader, key) {
            // There's no tidy way in rkv::Value to get an owned value. The
            // only way I can see to get such functionality is to go via
            // primitives.
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
        issuer: &str,
        serial: &str,
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&format!("{}:{}:{}", PREFIX_REV_IS, issuer, serial), state)
    }

    pub fn set_revocation_by_subject_and_pub_key(
        &mut self,
        subject: &str,
        pub_key_hash: &str,
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(
            &format!("{}:{}:{}", PREFIX_REV_SPK, subject, pub_key_hash),
            state,
        )
    }

    pub fn set_enrollment(
        &mut self,
        issuer: &str,
        serial: &str,
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&format!("{}:{}:{}", PREFIX_CRLITE, issuer, serial), state)
    }

    pub fn set_whitelist(
        &mut self,
        issuer: &str,
        serial: &str,
        state: i16,
    ) -> Result<(), SecurityStateError> {
        self.write_entry(&format!("{}:{}:{}", PREFIX_WL, issuer, serial), state)
    }

    pub fn get_revocation_state(
        &self,
        issuer: &str,
        serial: &str,
        subject: &str,
        pub_key: &str,
    ) -> Result<i16, SecurityStateError> {
        let pub_key_hash = match base64::decode(pub_key) {
            Ok(pk) => {
                let mut digest = Sha256::default();
                digest.input(&pk);
                let digest_result = digest.result();
                base64::encode(&digest_result)
            }
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem base64 decoding public key",
                ));
            }
        };

        let subject_pubkey = format!("{}:{}:{}", PREFIX_REV_SPK, subject, pub_key_hash);
        let issuer_serial = format!("{}:{}:{}", PREFIX_REV_IS, issuer, serial);

        let st: i16 = match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => value,
            Ok(None) => nsICertStorage::STATE_UNSET as i16,
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem reading revocation state (from issuer / serial)",
                ))
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
                ))
            }
        }
    }

    pub fn get_enrollment_state(
        &self,
        issuer: &str,
        serial: &str,
    ) -> Result<i16, SecurityStateError> {
        let issuer_serial = format!("{}:{}:{}", PREFIX_CRLITE, issuer, serial);
        match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => Ok(value),
            Ok(None) => Ok(nsICertStorage::STATE_UNSET as i16),
            Err(_) => return Err(SecurityStateError::from("problem reading enrollment state")),
        }
    }

    pub fn get_whitelist_state(
        &self,
        issuer: &str,
        serial: &str,
    ) -> Result<i16, SecurityStateError> {
        let issuer_serial = format!("{}:{}:{}", PREFIX_WL, issuer, serial);
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

fn do_construct_cert_storage(
    _outer: *const nsISupports,
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> Result<(), SecurityStateError> {
    let path_buf = match get_path_from_directory_service("ProfD") {
        Ok(path) => path,
        Err(_) => match get_path_from_directory_service("TmpD") {
            Ok(path) => path,
            Err(e) => return Err(e),
        },
    };

    let cert_storage = CertStorage::allocate(InitCertStorage {
        security_state: RwLock::new(SecurityState::new(path_buf)?),
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
            ))
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
    if !res.succeeded() {
        match res.0 {
            r if r == nsresult::NS_ERROR_UNEXPECTED as u32 => (),
            _ => return Err(SecurityStateError::from("could not read pref")), // nserror::nsresult(err),
        }
    }
    Ok(pref_value)
}

#[no_mangle]
pub extern "C" fn cert_storage_constructor(
    outer: *const nsISupports,
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nserror::nsresult {
    if !outer.is_null() {
        return nserror::NS_ERROR_NO_AGGREGATION;
    }

    match do_construct_cert_storage(outer, iid, result) {
        Ok(_) => nserror::NS_OK,
        Err(_) => {
            // In future: log something so we know what went wrong?
            nserror::NS_ERROR_FAILURE
        }
    }
}

#[derive(xpcom)]
#[xpimplements(nsICertStorage, nsIObserver)]
#[refcnt = "atomic"]

struct InitCertStorage {
    security_state: RwLock<SecurityState>,
}

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
                    let mut ss = match self.security_state.write() {
                        Err(_) => return Err(SecurityStateError::from("could not get write lock")),
                        Ok(write_guard) => write_guard,
                    };
                    ss.pref_seen(pref, up)
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
    ) -> nserror::nsresult {
        if issuer.is_null() || serial.is_null() {
            return nserror::NS_ERROR_FAILURE;
        }
        let mut ss = match self.security_state.write() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(write_guard) => write_guard,
        };
        match ss.set_revocation_by_issuer_and_serial(
            (*issuer).as_str_unchecked(),
            (*serial).as_str_unchecked(),
            state,
        ) {
            Ok(_) => nserror::NS_OK,
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn SetRevocationBySubjectAndPubKey(
        &self,
        subject: *const nsACString,
        pub_key_base64: *const nsACString,
        state: i16,
    ) -> nserror::nsresult {
        if subject.is_null() || pub_key_base64.is_null() {
            return nserror::NS_ERROR_FAILURE;
        }
        let mut ss = match self.security_state.write() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(write_guard) => write_guard,
        };
        match ss.set_revocation_by_subject_and_pub_key(
            (*subject).as_str_unchecked(),
            (*pub_key_base64).as_str_unchecked(),
            state,
        ) {
            Ok(_) => nserror::NS_OK,
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn SetEnrollment(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: i16,
    ) -> nserror::nsresult {
        if issuer.is_null() || serial.is_null() {
            return nserror::NS_ERROR_FAILURE;
        }
        let mut ss = match self.security_state.write() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(write_guard) => write_guard,
        };
        match ss.set_enrollment(
            (*issuer).as_str_unchecked(),
            (*serial).as_str_unchecked(),
            state,
        ) {
            Ok(_) => nserror::NS_OK,
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn SetWhitelist(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: i16,
    ) -> nserror::nsresult {
        if issuer.is_null() || serial.is_null() {
            return nserror::NS_ERROR_FAILURE;
        }
        let mut ss = match self.security_state.write() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(write_guard) => write_guard,
        };
        match ss.set_whitelist(
            (*issuer).as_str_unchecked(),
            (*serial).as_str_unchecked(),
            state,
        ) {
            Ok(_) => nserror::NS_OK,
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn GetRevocationState(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        subject: *const nsACString,
        pub_key_base64: *const nsACString,
        state: *mut i16,
    ) -> nserror::nsresult {
        if issuer.is_null() || serial.is_null() || subject.is_null() || pub_key_base64.is_null() {
            return nserror::NS_ERROR_FAILURE;
        }

        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *state = nsICertStorage::STATE_UNSET as i16;
        match ss.get_revocation_state(
            (*issuer).as_str_unchecked(),
            (*serial).as_str_unchecked(),
            (*subject).as_str_unchecked(),
            (*pub_key_base64).as_str_unchecked(),
        ) {
            Ok(st) => {
                *state = st;
                nserror::NS_OK
            }
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn GetEnrollmentState(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: *mut i16,
    ) -> nserror::nsresult {
        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *state = nsICertStorage::STATE_UNSET as i16;

        match ss.get_enrollment_state((*issuer).as_str_unchecked(), (*serial).as_str_unchecked()) {
            Ok(st) => {
                *state = st;
                nserror::NS_OK
            }
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn GetWhitelistState(
        &self,
        issuer: *const nsACString,
        serial: *const nsACString,
        state: *mut i16,
    ) -> nserror::nsresult {
        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *state = nsICertStorage::STATE_UNSET as i16;

        match ss.get_whitelist_state((*issuer).as_str_unchecked(), (*serial).as_str_unchecked()) {
            Ok(st) => {
                *state = st;
                nserror::NS_OK
            }
            _ => nserror::NS_ERROR_FAILURE,
        }
    }

    unsafe fn IsBlocklistFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;

        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *fresh = match ss.is_blocklist_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        nserror::NS_OK
    }

    unsafe fn IsWhitelistFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;

        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *fresh = match ss.is_whitelist_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        nserror::NS_OK
    }

    unsafe fn IsEnrollmentFresh(&self, fresh: *mut bool) -> nserror::nsresult {
        *fresh = false;

        let ss = match self.security_state.read() {
            Err(_) => return nserror::NS_ERROR_FAILURE,
            Ok(read_guard) => read_guard,
        };

        *fresh = match ss.is_enrollment_fresh() {
            Ok(is_fresh) => is_fresh,
            Err(_) => false,
        };

        nserror::NS_OK
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
                    _ => return nserror::NS_ERROR_FAILURE,
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
                    _ => return nserror::NS_ERROR_FAILURE,
                };

                let res = prefs.GetIntPref(
                    (&pref_name).as_ptr(),
                    (&mut pref_value) as *mut i32,
                );

                if !res.succeeded() {
                    return res;
                }

                let mut ss = match self.security_state.write() {
                    Err(_) => return nserror::NS_ERROR_FAILURE,
                    Ok(write_guard) => write_guard,
                };
                ss.pref_seen(name_string.as_str_unchecked(), pref_value);
            }
            _ => (),
        }
        nserror::NS_OK
    }
}
