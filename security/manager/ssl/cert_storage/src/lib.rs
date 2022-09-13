/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate base64;
extern crate byteorder;
extern crate crossbeam_utils;
#[macro_use]
extern crate cstr;
#[macro_use]
extern crate log;
extern crate moz_task;
extern crate nserror;
extern crate nsstring;
extern crate rkv;
extern crate rust_cascade;
extern crate sha2;
extern crate thin_vec;
extern crate time;
#[macro_use]
extern crate xpcom;
extern crate storage_variant;
extern crate tempfile;

extern crate wr_malloc_size_of;
use wr_malloc_size_of as malloc_size_of;

use byteorder::{LittleEndian, NetworkEndian, ReadBytesExt, WriteBytesExt};
use crossbeam_utils::atomic::AtomicCell;
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};
use moz_task::{create_background_task_queue, is_main_thread, Task, TaskRunnable};
use nserror::{
    nsresult, NS_ERROR_FAILURE, NS_ERROR_NOT_SAME_THREAD, NS_ERROR_NULL_POINTER,
    NS_ERROR_UNEXPECTED, NS_OK,
};
use nsstring::{nsACString, nsCStr, nsCString, nsString};
use rkv::backend::{BackendEnvironmentBuilder, SafeMode, SafeModeDatabase, SafeModeEnvironment};
use rkv::{StoreError, StoreOptions, Value};
use rust_cascade::Cascade;
use sha2::{Digest, Sha256};
use std::collections::{HashMap, HashSet};
use std::ffi::CString;
use std::fmt::Display;
use std::fs::{create_dir_all, remove_file, File, OpenOptions};
use std::io::{BufRead, BufReader, Read, Write};
use std::mem::size_of;
use std::path::{Path, PathBuf};
use std::str;
use std::sync::{Arc, RwLock};
use std::time::{SystemTime, UNIX_EPOCH};
use storage_variant::VariantType;
use thin_vec::ThinVec;
use xpcom::interfaces::{
    nsICRLiteCoverage, nsICRLiteTimestamp, nsICertInfo, nsICertStorage, nsICertStorageCallback,
    nsIFile, nsIHandleReportCallback, nsIIssuerAndSerialRevocationState, nsIMemoryReporter,
    nsIMemoryReporterManager, nsIProperties, nsIRevocationState, nsISerialEventTarget,
    nsISubjectAndPubKeyRevocationState, nsISupports,
};
use xpcom::{nsIID, GetterAddrefs, RefPtr, ThreadBoundRefPtr, XpCom};

const PREFIX_REV_IS: &str = "is";
const PREFIX_REV_SPK: &str = "spk";
const PREFIX_SUBJECT: &str = "subject";
const PREFIX_CERT: &str = "cert";
const PREFIX_DATA_TYPE: &str = "datatype";

const LAST_CRLITE_UPDATE_KEY: &str = "last_crlite_update";

const COVERAGE_SERIALIZATION_VERSION: u8 = 1;
const COVERAGE_V1_ENTRY_BYTES: usize = 48;

const ENROLLMENT_SERIALIZATION_VERSION: u8 = 1;
const ENROLLMENT_V1_ENTRY_BYTES: usize = 32;

type Rkv = rkv::Rkv<SafeModeEnvironment>;
type SingleStore = rkv::SingleStore<SafeModeDatabase>;

macro_rules! make_key {
    ( $prefix:expr, $( $part:expr ),+ ) => {
        {
            let mut key = $prefix.as_bytes().to_owned();
            $( key.extend_from_slice($part); )+
            key
        }
    }
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

impl MallocSizeOf for EnvAndStore {
    fn size_of(&self, _ops: &mut MallocSizeOfOps) -> usize {
        self.env
            .read()
            .and_then(|reader| {
                let iter = self.store.iter_start(&reader)?.into_iter();
                Ok(iter
                    .map(|r| {
                        r.map(|(k, v)| k.len() + v.serialized_size().unwrap_or(0) as usize)
                            .unwrap_or(0)
                    })
                    .sum())
            })
            .unwrap_or(0)
    }
}

/// `SecurityState`
struct SecurityState {
    profile_path: PathBuf,
    env_and_store: Option<EnvAndStore>,
    crlite_filter: Option<Cascade>,
    /// Maps issuer spki hashes to sets of serial numbers.
    crlite_stash: Option<HashMap<Vec<u8>, HashSet<Vec<u8>>>>,
    /// Maps an RFC 6962 LogID to a pair of 64 bit unix timestamps
    crlite_coverage: Option<HashMap<Vec<u8>, (u64, u64)>>,
    /// Set of `SHA256(subject || spki)` values for enrolled issuers
    crlite_enrollment: Option<HashSet<Vec<u8>>>,
    /// Tracks the number of asynchronous operations which have been dispatched but not completed.
    remaining_ops: i32,
}

impl SecurityState {
    pub fn new(profile_path: PathBuf) -> SecurityState {
        // Since this gets called on the main thread, we don't actually want to open the DB yet.
        // We do this on-demand later, when we're probably on a certificate verification thread.
        SecurityState {
            profile_path,
            env_and_store: None,
            crlite_filter: None,
            crlite_stash: None,
            crlite_coverage: None,
            crlite_enrollment: None,
            remaining_ops: 0,
        }
    }

    pub fn db_needs_opening(&self) -> bool {
        self.env_and_store.is_none()
    }

    pub fn open_db(&mut self) -> Result<(), SecurityStateError> {
        if self.env_and_store.is_some() {
            return Ok(());
        }

        let store_path = get_store_path(&self.profile_path)?;

        // Open the store in read-write mode to create it (if needed) and migrate data from the old
        // store (if any).
        // If opening initially fails, try to remove and recreate the database. Consumers will
        // repopulate the database as necessary if this happens (see bug 1546361).
        let env = make_env(store_path.as_path()).or_else(|_| {
            remove_db(store_path.as_path())?;
            make_env(store_path.as_path())
        })?;
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
        self.load_crlite_filter()?;
        Ok(())
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
                        &make_key!(PREFIX_REV_SPK, name, &l_sans_prefix),
                        &value,
                    );
                } else {
                    let _ = store.put(
                        &mut writer,
                        &make_key!(PREFIX_REV_IS, name, &l_sans_prefix),
                        &value,
                    );
                }
            }
        }

        writer.commit()?;
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
            Ok(_) => Err(SecurityStateError::from(
                "Unexpected type when trying to get a Value::I64",
            )),
            Err(_) => Err(SecurityStateError::from(
                "There was a problem getting the value",
            )),
        }
    }

    pub fn get_has_prior_data(&self, data_type: u8) -> Result<bool, SecurityStateError> {
        if data_type == nsICertStorage::DATA_TYPE_CRLITE_FILTER_FULL {
            return Ok(self.crlite_filter.is_some()
                && self.crlite_coverage.is_some()
                && self.crlite_enrollment.is_some());
        }
        if data_type == nsICertStorage::DATA_TYPE_CRLITE_FILTER_INCREMENTAL {
            return Ok(self.crlite_stash.is_some());
        }

        let env_and_store = match self.env_and_store.as_ref() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let reader = env_and_store.env.read()?;
        match env_and_store
            .store
            .get(&reader, &make_key!(PREFIX_DATA_TYPE, &[data_type]))
        {
            Ok(Some(Value::Bool(true))) => Ok(true),
            Ok(None) => Ok(false),
            Ok(_) => Err(SecurityStateError::from(
                "Unexpected type when trying to get a Value::Bool",
            )),
            Err(_) => Err(SecurityStateError::from(
                "There was a problem getting the value",
            )),
        }
    }

    pub fn set_batch_state(
        &mut self,
        entries: &[EncodedSecurityState],
        typ: u8,
    ) -> Result<(), SecurityStateError> {
        let env_and_store = match self.env_and_store.as_mut() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let mut writer = env_and_store.env.write()?;
        // Make a note that we have prior data of the given type now.
        env_and_store.store.put(
            &mut writer,
            &make_key!(PREFIX_DATA_TYPE, &[typ]),
            &Value::Bool(true),
        )?;

        for entry in entries {
            let key = match entry.key() {
                Ok(key) => key,
                Err(e) => {
                    warn!("error base64-decoding key parts - ignoring: {}", e.message);
                    continue;
                }
            };
            env_and_store
                .store
                .put(&mut writer, &key, &Value::I64(entry.state() as i64))?;
        }

        writer.commit()?;
        Ok(())
    }

    pub fn get_revocation_state(
        &self,
        issuer: &[u8],
        serial: &[u8],
        subject: &[u8],
        pub_key: &[u8],
    ) -> Result<i16, SecurityStateError> {
        let mut digest = Sha256::default();
        digest.update(pub_key);
        let pub_key_hash = digest.finalize();

        let subject_pubkey = make_key!(PREFIX_REV_SPK, subject, &pub_key_hash);
        let issuer_serial = make_key!(PREFIX_REV_IS, issuer, serial);

        let st: i16 = match self.read_entry(&issuer_serial) {
            Ok(Some(value)) => value,
            Ok(None) => nsICertStorage::STATE_UNSET,
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem reading revocation state (from issuer / serial)",
                ));
            }
        };

        if st != nsICertStorage::STATE_UNSET {
            return Ok(st);
        }

        match self.read_entry(&subject_pubkey) {
            Ok(Some(value)) => Ok(value),
            Ok(None) => Ok(nsICertStorage::STATE_UNSET),
            Err(_) => {
                return Err(SecurityStateError::from(
                    "problem reading revocation state (from subject / pubkey)",
                ));
            }
        }
    }

    fn issuer_is_enrolled(&self, subject: &[u8], pub_key: &[u8]) -> bool {
        if let Some(crlite_enrollment) = self.crlite_enrollment.as_ref() {
            let mut digest = Sha256::default();
            digest.update(subject);
            digest.update(pub_key);
            let issuer_id = digest.finalize();
            return crlite_enrollment.contains(&issuer_id.to_vec());
        }
        return false;
    }

    fn filter_covers_some_timestamp(&self, timestamps: &[CRLiteTimestamp]) -> bool {
        if let Some(crlite_coverage) = self.crlite_coverage.as_ref() {
            for entry in timestamps {
                if let Some(&(low, high)) = crlite_coverage.get(entry.log_id.as_ref()) {
                    if low <= entry.timestamp && entry.timestamp <= high {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    fn note_crlite_update_time(&mut self) -> Result<(), SecurityStateError> {
        let seconds_since_epoch = Value::U64(
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map_err(|_| SecurityStateError::from("could not get current time"))?
                .as_secs(),
        );
        let env_and_store = match self.env_and_store.as_mut() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let mut writer = env_and_store.env.write()?;
        env_and_store
            .store
            .put(&mut writer, LAST_CRLITE_UPDATE_KEY, &seconds_since_epoch)
            .map_err(|_| SecurityStateError::from("could not store timestamp"))?;
        writer.commit()?;
        Ok(())
    }

    fn is_crlite_fresh(&self) -> bool {
        let now = match SystemTime::now().duration_since(UNIX_EPOCH) {
            Ok(t) => t.as_secs(),
            _ => return false,
        };
        let env_and_store = match self.env_and_store.as_ref() {
            Some(env_and_store) => env_and_store,
            None => return false,
        };
        let reader = match env_and_store.env.read() {
            Ok(reader) => reader,
            _ => return false,
        };
        match env_and_store.store.get(&reader, LAST_CRLITE_UPDATE_KEY) {
            Ok(Some(Value::U64(last_update))) if last_update < u64::MAX / 2 => {
                now < last_update + 60 * 60 * 24 * 10
            }
            _ => false,
        }
    }

    pub fn set_full_crlite_filter(
        &mut self,
        filter: Vec<u8>,
        enrolled_issuers: Vec<nsCString>,
        coverage_entries: &[(nsCString, u64, u64)],
    ) -> Result<(), SecurityStateError> {
        // First drop any existing crlite filter and clear the accumulated stash.
        {
            let _ = self.crlite_filter.take();
            let _ = self.crlite_stash.take();
            let _ = self.crlite_coverage.take();
            let _ = self.crlite_enrollment.take();
            let mut path = get_store_path(&self.profile_path)?;
            path.push("crlite.stash");
            // Truncate the stash file if it exists.
            if path.exists() {
                File::create(path).map_err(|e| {
                    SecurityStateError::from(format!("couldn't truncate stash file: {}", e))
                })?;
            }
        }
        // Write the new full filter.
        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.filter");
        {
            let mut filter_file = File::create(&path)?;
            filter_file.write_all(&filter)?;
        }

        // Serialize the coverage metadata as a 1 byte version number followed by any number of 48
        // byte entries. Each entry is a 32 byte (opaque) log id, followed by two 8 byte
        // timestamps. Each timestamp is an 8 byte unsigned integer in little endian.
        let mut coverage_bytes =
            Vec::with_capacity(size_of::<u8>() + coverage_entries.len() * COVERAGE_V1_ENTRY_BYTES);
        coverage_bytes.push(COVERAGE_SERIALIZATION_VERSION);
        for (b64_log_id, min_t, max_t) in coverage_entries {
            let log_id = match base64::decode(&b64_log_id) {
                Ok(log_id) if log_id.len() == 32 => log_id,
                _ => {
                    warn!("malformed log ID - skipping: {}", b64_log_id);
                    continue;
                }
            };
            coverage_bytes.extend_from_slice(&log_id);
            coverage_bytes.extend_from_slice(&min_t.to_le_bytes());
            coverage_bytes.extend_from_slice(&max_t.to_le_bytes());
        }
        // Write the coverage file for the new filter
        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.coverage");
        {
            let mut coverage_file = File::create(&path)?;
            coverage_file.write_all(&coverage_bytes)?;
        }

        // Serialize the enrollment list as a 1 byte version number followed by:
        // Version 1: any number of 32 byte values of the form `SHA256(subject || spki)`.
        let mut enrollment_bytes = Vec::with_capacity(
            size_of::<u8>() + enrolled_issuers.len() * ENROLLMENT_V1_ENTRY_BYTES,
        );
        enrollment_bytes.push(ENROLLMENT_SERIALIZATION_VERSION);
        for b64_issuer_id in enrolled_issuers {
            let issuer_id = match base64::decode(&b64_issuer_id) {
                Ok(issuer_id) if issuer_id.len() == 32 => issuer_id,
                _ => {
                    warn!("malformed issuer ID - skipping: {}", b64_issuer_id);
                    continue;
                }
            };
            enrollment_bytes.extend_from_slice(&issuer_id);
        }
        // Write the enrollment file for the new filter
        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.enrollment");
        {
            let mut enrollment_file = File::create(&path)?;
            enrollment_file.write_all(&enrollment_bytes)?;
        }

        self.note_crlite_update_time()?;
        self.load_crlite_filter()?;
        Ok(())
    }

    fn load_crlite_filter(&mut self) -> Result<(), SecurityStateError> {
        if self.crlite_filter.is_some() || self.crlite_coverage.is_some() {
            return Err(SecurityStateError::from(
                "Both crlite_filter and crlite_coverage should be None here",
            ));
        }

        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.filter");
        // Before we've downloaded any filters, this file won't exist.
        if !path.exists() {
            return Ok(());
        }
        let mut filter_file = File::open(path)?;
        let mut filter_bytes = Vec::new();
        let _ = filter_file.read_to_end(&mut filter_bytes)?;
        let crlite_filter = Cascade::from_bytes(filter_bytes)
            .map_err(|_| SecurityStateError::from("invalid CRLite filter"))?
            .ok_or(SecurityStateError::from("expecting non-empty filter"))?;

        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.coverage");
        if !path.exists() {
            return Ok(());
        }

        // Deserialize the coverage metadata.
        // The format is described in `set_full_crlite_filter`.
        let coverage_file = File::open(path)?;
        let coverage_file_len = coverage_file.metadata()?.len() as usize;
        let mut coverage_reader = BufReader::new(coverage_file);
        match coverage_reader.read_u8() {
            Ok(COVERAGE_SERIALIZATION_VERSION) => (),
            _ => return Err(SecurityStateError::from("unknown CRLite coverage version")),
        }
        if (coverage_file_len - 1) % COVERAGE_V1_ENTRY_BYTES != 0 {
            return Err(SecurityStateError::from("truncated CRLite coverage file"));
        }
        let coverage_count = (coverage_file_len - 1) / COVERAGE_V1_ENTRY_BYTES;
        let mut crlite_coverage: HashMap<Vec<u8>, (u64, u64)> = HashMap::new();
        for _ in 0..coverage_count {
            let mut coverage_entry = [0u8; COVERAGE_V1_ENTRY_BYTES];
            match coverage_reader.read_exact(&mut coverage_entry) {
                Ok(()) => (),
                _ => return Err(SecurityStateError::from("truncated CRLite coverage file")),
            };
            let log_id = &coverage_entry[0..32];
            let min_timestamp: u64;
            let max_timestamp: u64;
            match (&coverage_entry[32..40]).read_u64::<LittleEndian>() {
                Ok(value) => min_timestamp = value,
                _ => return Err(SecurityStateError::from("truncated CRLite coverage file")),
            }
            match (&coverage_entry[40..48]).read_u64::<LittleEndian>() {
                Ok(value) => max_timestamp = value,
                _ => return Err(SecurityStateError::from("truncated CRLite coverage file")),
            }
            crlite_coverage.insert(log_id.to_vec(), (min_timestamp, max_timestamp));
        }

        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.enrollment");
        if !path.exists() {
            return Ok(());
        }

        // Deserialize the enrollment metadata.
        // The format is described in `set_full_crlite_filter`.
        let enrollment_file = File::open(path)?;
        let enrollment_file_len = enrollment_file.metadata()?.len() as usize;
        let mut enrollment_reader = BufReader::new(enrollment_file);
        match enrollment_reader.read_u8() {
            Ok(ENROLLMENT_SERIALIZATION_VERSION) => (),
            _ => {
                return Err(SecurityStateError::from(
                    "unknown CRLite enrollment version",
                ))
            }
        }
        if (enrollment_file_len - 1) % ENROLLMENT_V1_ENTRY_BYTES != 0 {
            return Err(SecurityStateError::from("truncated CRLite enrollment file"));
        }
        let enrollment_count = (enrollment_file_len - 1) / ENROLLMENT_V1_ENTRY_BYTES;
        let mut crlite_enrollment: HashSet<Vec<u8>> = HashSet::new();
        for _ in 0..enrollment_count {
            let mut enrollment_entry = [0u8; ENROLLMENT_V1_ENTRY_BYTES];
            match enrollment_reader.read_exact(&mut enrollment_entry) {
                Ok(()) => (),
                _ => return Err(SecurityStateError::from("truncated CRLite enrollment file")),
            };
            let issuer_id = &enrollment_entry[..];
            crlite_enrollment.insert(issuer_id.to_vec());
        }

        let old_crlite_filter_should_be_none = self.crlite_filter.replace(crlite_filter);
        assert!(old_crlite_filter_should_be_none.is_none());
        let old_crlite_coverage_should_be_none = self.crlite_coverage.replace(crlite_coverage);
        assert!(old_crlite_coverage_should_be_none.is_none());
        let old_crlite_enrollment_should_be_none =
            self.crlite_enrollment.replace(crlite_enrollment);
        assert!(old_crlite_enrollment_should_be_none.is_none());
        Ok(())
    }

    pub fn add_crlite_stash(&mut self, stash: Vec<u8>) -> Result<(), SecurityStateError> {
        // Append the update to the previously-seen stashes.
        let mut path = get_store_path(&self.profile_path)?;
        path.push("crlite.stash");
        let mut stash_file = OpenOptions::new().append(true).create(true).open(path)?;
        stash_file.write_all(&stash)?;
        let crlite_stash = self.crlite_stash.get_or_insert(HashMap::new());
        load_crlite_stash_from_reader_into_map(&mut stash.as_slice(), crlite_stash)?;
        self.note_crlite_update_time()?;
        Ok(())
    }

    pub fn is_cert_revoked_by_stash(
        &self,
        issuer_spki: &[u8],
        serial: &[u8],
    ) -> Result<bool, SecurityStateError> {
        let crlite_stash = match self.crlite_stash.as_ref() {
            Some(crlite_stash) => crlite_stash,
            None => return Ok(false),
        };
        let mut digest = Sha256::default();
        digest.update(issuer_spki);
        let lookup_key = digest.finalize().to_vec();
        let serials = match crlite_stash.get(&lookup_key) {
            Some(serials) => serials,
            None => return Ok(false),
        };
        Ok(serials.contains(&serial.to_vec()))
    }

    pub fn get_crlite_revocation_state(
        &self,
        issuer: &[u8],
        issuer_spki: &[u8],
        serial_number: &[u8],
        timestamps: &[CRLiteTimestamp],
    ) -> i16 {
        if !self.is_crlite_fresh() {
            return nsICertStorage::STATE_NO_FILTER;
        }
        if !self.issuer_is_enrolled(issuer, issuer_spki) {
            return nsICertStorage::STATE_NOT_ENROLLED;
        }
        if !self.filter_covers_some_timestamp(timestamps) {
            return nsICertStorage::STATE_NOT_COVERED;
        }
        let mut digest = Sha256::default();
        digest.update(issuer_spki);
        let mut lookup_key = digest.finalize().to_vec();
        lookup_key.extend_from_slice(serial_number);
        debug!("CRLite lookup key: {:?}", lookup_key);
        let result = match &self.crlite_filter {
            Some(crlite_filter) => crlite_filter.has(lookup_key),
            // This can only happen if the backing file was deleted or if it or our database has
            // become corrupted. In any case, we have no information.
            None => return nsICertStorage::STATE_NO_FILTER,
        };
        match result {
            true => nsICertStorage::STATE_ENFORCE,
            false => nsICertStorage::STATE_UNSET,
        }
    }

    // To store certificates, we create a Cert out of each given cert, subject, and trust tuple. We
    // hash each certificate with sha-256 to obtain a unique* key for that certificate, and we store
    // the Cert in the database. We also look up or create a CertHashList for the given subject and
    // add the new certificate's hash if it isn't present in the list. If it wasn't present, we
    // write out the updated CertHashList.
    // *By the pigeon-hole principle, there exist collisions for sha-256, so this key is not
    // actually unique. We rely on the assumption that sha-256 is a cryptographically strong hash.
    // If an adversary can find two different certificates with the same sha-256 hash, they can
    // probably forge a sha-256-based signature, so assuming the keys we create here are unique is
    // not a security issue.
    pub fn add_certs(
        &mut self,
        certs: &[(nsCString, nsCString, i16)],
    ) -> Result<(), SecurityStateError> {
        let env_and_store = match self.env_and_store.as_mut() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let mut writer = env_and_store.env.write()?;
        // Make a note that we have prior cert data now.
        env_and_store.store.put(
            &mut writer,
            &make_key!(PREFIX_DATA_TYPE, &[nsICertStorage::DATA_TYPE_CERTIFICATE]),
            &Value::Bool(true),
        )?;

        for (cert_der_base64, subject_base64, trust) in certs {
            let cert_der = match base64::decode(&cert_der_base64) {
                Ok(cert_der) => cert_der,
                Err(e) => {
                    warn!("error base64-decoding cert - skipping: {}", e);
                    continue;
                }
            };
            let subject = match base64::decode(&subject_base64) {
                Ok(subject) => subject,
                Err(e) => {
                    warn!("error base64-decoding subject - skipping: {}", e);
                    continue;
                }
            };
            let mut digest = Sha256::default();
            digest.update(&cert_der);
            let cert_hash = digest.finalize();
            let cert_key = make_key!(PREFIX_CERT, &cert_hash);
            let cert = Cert::new(&cert_der, &subject, *trust)?;
            env_and_store
                .store
                .put(&mut writer, &cert_key, &Value::Blob(&cert.to_bytes()?))?;
            let subject_key = make_key!(PREFIX_SUBJECT, &subject);
            let empty_vec = Vec::new();
            let old_cert_hash_list = match env_and_store.store.get(&writer, &subject_key)? {
                Some(Value::Blob(hashes)) => hashes.to_owned(),
                Some(_) => empty_vec,
                None => empty_vec,
            };
            let new_cert_hash_list = CertHashList::add(&old_cert_hash_list, &cert_hash)?;
            if new_cert_hash_list.len() != old_cert_hash_list.len() {
                env_and_store.store.put(
                    &mut writer,
                    &subject_key,
                    &Value::Blob(&new_cert_hash_list),
                )?;
            }
        }

        writer.commit()?;
        Ok(())
    }

    // Given a list of certificate sha-256 hashes, we can look up each Cert entry in the database.
    // We use this to find the corresponding subject so we can look up the CertHashList it should
    // appear in. If that list contains the given hash, we remove it and update the CertHashList.
    // Finally we delete the Cert entry.
    pub fn remove_certs_by_hashes(
        &mut self,
        hashes_base64: &[nsCString],
    ) -> Result<(), SecurityStateError> {
        let env_and_store = match self.env_and_store.as_mut() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let mut writer = env_and_store.env.write()?;
        let reader = env_and_store.env.read()?;

        for hash in hashes_base64 {
            let hash = match base64::decode(&hash) {
                Ok(hash) => hash,
                Err(e) => {
                    warn!("error decoding hash - ignoring: {}", e);
                    continue;
                }
            };
            let cert_key = make_key!(PREFIX_CERT, &hash);
            if let Some(Value::Blob(cert_bytes)) = env_and_store.store.get(&reader, &cert_key)? {
                if let Ok(cert) = Cert::from_bytes(cert_bytes) {
                    let subject_key = make_key!(PREFIX_SUBJECT, &cert.subject);
                    let empty_vec = Vec::new();
                    // We have to use the writer here to make sure we have an up-to-date view of
                    // the cert hash list.
                    let old_cert_hash_list = match env_and_store.store.get(&writer, &subject_key)? {
                        Some(Value::Blob(hashes)) => hashes.to_owned(),
                        Some(_) => empty_vec,
                        None => empty_vec,
                    };
                    let new_cert_hash_list = CertHashList::remove(&old_cert_hash_list, &hash)?;
                    if new_cert_hash_list.len() != old_cert_hash_list.len() {
                        env_and_store.store.put(
                            &mut writer,
                            &subject_key,
                            &Value::Blob(&new_cert_hash_list),
                        )?;
                    }
                }
            }
            match env_and_store.store.delete(&mut writer, &cert_key) {
                Ok(()) => {}
                Err(StoreError::KeyValuePairNotFound) => {}
                Err(e) => return Err(SecurityStateError::from(e)),
            };
        }
        writer.commit()?;
        Ok(())
    }

    // Given a certificate's subject, we look up the corresponding CertHashList. In theory, each
    // hash in that list corresponds to a certificate with the given subject, so we look up each of
    // these (assuming the database is consistent and contains them) and add them to the given list.
    // If we encounter an inconsistency, we continue looking as best we can.
    pub fn find_certs_by_subject(
        &self,
        subject: &[u8],
        certs: &mut ThinVec<ThinVec<u8>>,
    ) -> Result<(), SecurityStateError> {
        let env_and_store = match self.env_and_store.as_ref() {
            Some(env_and_store) => env_and_store,
            None => return Err(SecurityStateError::from("env and store not initialized?")),
        };
        let reader = env_and_store.env.read()?;
        certs.clear();
        let subject_key = make_key!(PREFIX_SUBJECT, subject);
        let empty_vec = Vec::new();
        let cert_hash_list_bytes = match env_and_store.store.get(&reader, &subject_key)? {
            Some(Value::Blob(hashes)) => hashes,
            Some(_) => &empty_vec,
            None => &empty_vec,
        };
        let cert_hash_list = CertHashList::new(cert_hash_list_bytes)?;
        for cert_hash in cert_hash_list.into_iter() {
            let cert_key = make_key!(PREFIX_CERT, cert_hash);
            // If there's some inconsistency, we don't want to fail the whole operation - just go
            // for best effort and find as many certificates as we can.
            if let Some(Value::Blob(cert_bytes)) = env_and_store.store.get(&reader, &cert_key)? {
                if let Ok(cert) = Cert::from_bytes(cert_bytes) {
                    let mut thin_vec_cert = ThinVec::with_capacity(cert.der.len());
                    thin_vec_cert.extend_from_slice(&cert.der);
                    certs.push(thin_vec_cert);
                }
            }
        }
        Ok(())
    }
}

impl MallocSizeOf for SecurityState {
    fn size_of(&self, ops: &mut MallocSizeOfOps) -> usize {
        self.profile_path.size_of(ops)
            + self.env_and_store.size_of(ops)
            + self
                .crlite_filter
                .as_ref()
                .map_or(0, |crlite_filter| crlite_filter.approximate_size_of())
            + self.crlite_stash.size_of(ops)
            + self.crlite_coverage.size_of(ops)
            + self.remaining_ops.size_of(ops)
    }
}

const CERT_SERIALIZATION_VERSION_1: u8 = 1;

// A Cert consists of its DER encoding, its DER-encoded subject, and its trust (currently
// nsICertStorage::TRUST_INHERIT, but in the future nsICertStorage::TRUST_ANCHOR may also be used).
// The length of each encoding must be representable by a u16 (so 65535 bytes is the longest a
// certificate can be).
struct Cert<'a> {
    der: &'a [u8],
    subject: &'a [u8],
    trust: i16,
}

impl<'a> Cert<'a> {
    fn new(der: &'a [u8], subject: &'a [u8], trust: i16) -> Result<Cert<'a>, SecurityStateError> {
        if der.len() > u16::max as usize {
            return Err(SecurityStateError::from("certificate is too long"));
        }
        if subject.len() > u16::max as usize {
            return Err(SecurityStateError::from("subject is too long"));
        }
        Ok(Cert {
            der,
            subject,
            trust,
        })
    }

    fn from_bytes(encoded: &'a [u8]) -> Result<Cert<'a>, SecurityStateError> {
        if encoded.len() < size_of::<u8>() {
            return Err(SecurityStateError::from("invalid Cert: no version?"));
        }
        let (mut version, rest) = encoded.split_at(size_of::<u8>());
        let version = version.read_u8()?;
        if version != CERT_SERIALIZATION_VERSION_1 {
            return Err(SecurityStateError::from("invalid Cert: unexpected version"));
        }

        if rest.len() < size_of::<u16>() {
            return Err(SecurityStateError::from("invalid Cert: no der len?"));
        }
        let (mut der_len, rest) = rest.split_at(size_of::<u16>());
        let der_len = der_len.read_u16::<NetworkEndian>()? as usize;
        if rest.len() < der_len {
            return Err(SecurityStateError::from("invalid Cert: no der?"));
        }
        let (der, rest) = rest.split_at(der_len);

        if rest.len() < size_of::<u16>() {
            return Err(SecurityStateError::from("invalid Cert: no subject len?"));
        }
        let (mut subject_len, rest) = rest.split_at(size_of::<u16>());
        let subject_len = subject_len.read_u16::<NetworkEndian>()? as usize;
        if rest.len() < subject_len {
            return Err(SecurityStateError::from("invalid Cert: no subject?"));
        }
        let (subject, mut rest) = rest.split_at(subject_len);

        if rest.len() < size_of::<i16>() {
            return Err(SecurityStateError::from("invalid Cert: no trust?"));
        }
        let trust = rest.read_i16::<NetworkEndian>()?;
        if rest.len() > 0 {
            return Err(SecurityStateError::from("invalid Cert: trailing data?"));
        }

        Ok(Cert {
            der,
            subject,
            trust,
        })
    }

    fn to_bytes(&self) -> Result<Vec<u8>, SecurityStateError> {
        let mut bytes = Vec::with_capacity(
            size_of::<u8>()
                + size_of::<u16>()
                + self.der.len()
                + size_of::<u16>()
                + self.subject.len()
                + size_of::<i16>(),
        );
        bytes.write_u8(CERT_SERIALIZATION_VERSION_1)?;
        if self.der.len() > u16::max as usize {
            return Err(SecurityStateError::from("certificate is too long"));
        }
        bytes.write_u16::<NetworkEndian>(self.der.len() as u16)?;
        bytes.extend_from_slice(&self.der);
        if self.subject.len() > u16::max as usize {
            return Err(SecurityStateError::from("subject is too long"));
        }
        bytes.write_u16::<NetworkEndian>(self.subject.len() as u16)?;
        bytes.extend_from_slice(&self.subject);
        bytes.write_i16::<NetworkEndian>(self.trust)?;
        Ok(bytes)
    }
}

// A CertHashList is a list of sha-256 hashes of DER-encoded certificates.
struct CertHashList<'a> {
    hashes: Vec<&'a [u8]>,
}

impl<'a> CertHashList<'a> {
    fn new(hashes_bytes: &'a [u8]) -> Result<CertHashList<'a>, SecurityStateError> {
        if hashes_bytes.len() % Sha256::output_size() != 0 {
            return Err(SecurityStateError::from(
                "unexpected length for cert hash list",
            ));
        }
        let mut hashes = Vec::with_capacity(hashes_bytes.len() / Sha256::output_size());
        for hash in hashes_bytes.chunks_exact(Sha256::output_size()) {
            hashes.push(hash);
        }
        Ok(CertHashList { hashes })
    }

    fn add(hashes_bytes: &[u8], new_hash: &[u8]) -> Result<Vec<u8>, SecurityStateError> {
        if hashes_bytes.len() % Sha256::output_size() != 0 {
            return Err(SecurityStateError::from(
                "unexpected length for cert hash list",
            ));
        }
        if new_hash.len() != Sha256::output_size() {
            return Err(SecurityStateError::from("unexpected cert hash length"));
        }
        for hash in hashes_bytes.chunks_exact(Sha256::output_size()) {
            if hash == new_hash {
                return Ok(hashes_bytes.to_owned());
            }
        }
        let mut combined = hashes_bytes.to_owned();
        combined.extend_from_slice(new_hash);
        Ok(combined)
    }

    fn remove(hashes_bytes: &[u8], cert_hash: &[u8]) -> Result<Vec<u8>, SecurityStateError> {
        if hashes_bytes.len() % Sha256::output_size() != 0 {
            return Err(SecurityStateError::from(
                "unexpected length for cert hash list",
            ));
        }
        if cert_hash.len() != Sha256::output_size() {
            return Err(SecurityStateError::from("unexpected cert hash length"));
        }
        let mut result = Vec::with_capacity(hashes_bytes.len());
        for hash in hashes_bytes.chunks_exact(Sha256::output_size()) {
            if hash != cert_hash {
                result.extend_from_slice(hash);
            }
        }
        Ok(result)
    }
}

impl<'a> IntoIterator for CertHashList<'a> {
    type Item = &'a [u8];
    type IntoIter = std::vec::IntoIter<&'a [u8]>;

    fn into_iter(self) -> Self::IntoIter {
        self.hashes.into_iter()
    }
}

// Helper struct for get_crlite_revocation_state.
struct CRLiteTimestamp {
    log_id: ThinVec<u8>,
    timestamp: u64,
}

// Helper struct for set_batch_state. Takes a prefix, two base64-encoded key
// parts, and a security state value.
struct EncodedSecurityState {
    prefix: &'static str,
    key_part_1_base64: nsCString,
    key_part_2_base64: nsCString,
    state: i16,
}

impl EncodedSecurityState {
    fn new(
        prefix: &'static str,
        key_part_1_base64: nsCString,
        key_part_2_base64: nsCString,
        state: i16,
    ) -> EncodedSecurityState {
        EncodedSecurityState {
            prefix,
            key_part_1_base64,
            key_part_2_base64,
            state,
        }
    }

    fn key(&self) -> Result<Vec<u8>, SecurityStateError> {
        let key_part_1 = base64::decode(&self.key_part_1_base64)?;
        let key_part_2 = base64::decode(&self.key_part_2_base64)?;
        Ok(make_key!(self.prefix, &key_part_1, &key_part_2))
    }

    fn state(&self) -> i16 {
        self.state
    }
}

fn get_path_from_directory_service(key: &str) -> Result<PathBuf, nserror::nsresult> {
    let directory_service: RefPtr<nsIProperties> =
        xpcom::components::Directory::service().map_err(|_| NS_ERROR_FAILURE)?;
    let cs_key = CString::new(key).map_err(|_| NS_ERROR_FAILURE)?;

    let mut requested_dir = GetterAddrefs::<nsIFile>::new();
    unsafe {
        (*directory_service)
            .Get(
                (&cs_key).as_ptr(),
                &nsIFile::IID as *const nsIID,
                requested_dir.void_ptr(),
            )
            .to_result()
    }?;

    let dir_path = requested_dir.refptr().ok_or(NS_ERROR_FAILURE)?;
    let mut path = nsString::new();
    unsafe { (*dir_path).GetPath(&mut *path).to_result() }?;
    Ok(PathBuf::from(format!("{}", path)))
}

fn get_profile_path() -> Result<PathBuf, nserror::nsresult> {
    get_path_from_directory_service("ProfD").or_else(|_| get_path_from_directory_service("TmpD"))
}

fn get_store_path(profile_path: &PathBuf) -> Result<PathBuf, SecurityStateError> {
    let mut store_path = profile_path.clone();
    store_path.push("security_state");
    create_dir_all(store_path.as_path())?;
    Ok(store_path)
}

fn make_env(path: &Path) -> Result<Rkv, SecurityStateError> {
    let mut builder = Rkv::environment_builder::<SafeMode>();
    builder.set_max_dbs(2);

    // 16MB is a little over twice the size of the current dataset. When we
    // eventually switch to the LMDB backend to create the builder above,
    // we should set this as the map size, since it cannot currently resize.
    // (The SafeMode backend warns when a map size is specified, so we skip it
    // for now to avoid console spam.)

    // builder.set_map_size(16777216);

    // Bug 1595004: Migrate databases between backends in the future,
    // and handle 32 and 64 bit architectures in case of LMDB.
    Rkv::from_builder(path, builder).map_err(SecurityStateError::from)
}

fn unconditionally_remove_file(path: &Path) -> Result<(), SecurityStateError> {
    match remove_file(path) {
        Ok(()) => Ok(()),
        Err(e) => match e.kind() {
            std::io::ErrorKind::NotFound => Ok(()),
            _ => Err(SecurityStateError::from(e)),
        },
    }
}

fn remove_db(path: &Path) -> Result<(), SecurityStateError> {
    // Remove LMDB-related files.
    let db = path.join("data.mdb");
    unconditionally_remove_file(&db)?;
    let lock = path.join("lock.mdb");
    unconditionally_remove_file(&lock)?;

    // Remove SafeMode-related files.
    let db = path.join("data.safe.bin");
    unconditionally_remove_file(&db)?;

    Ok(())
}

// Helper function to read stash information from the given reader and insert the results into the
// given stash map.
fn load_crlite_stash_from_reader_into_map(
    reader: &mut dyn Read,
    dest: &mut HashMap<Vec<u8>, HashSet<Vec<u8>>>,
) -> Result<(), SecurityStateError> {
    // The basic unit of the stash file is an issuer subject public key info
    // hash (sha-256) followed by a number of serial numbers corresponding
    // to revoked certificates issued by that issuer. More specifically,
    // each unit consists of:
    //   4 bytes little-endian: the number of serial numbers following the issuer spki hash
    //   1 byte: the length of the issuer spki hash
    //   issuer spki hash length bytes: the issuer spki hash
    //   as many times as the indicated serial numbers:
    //     1 byte: the length of the serial number
    //     serial number length bytes: the serial number
    // The stash file consists of any number of these units concatenated
    // together.
    loop {
        let num_serials = match reader.read_u32::<LittleEndian>() {
            Ok(num_serials) => num_serials,
            Err(_) => break, // end of input, presumably
        };
        let issuer_spki_hash_len = reader.read_u8().map_err(|e| {
            SecurityStateError::from(format!("error reading stash issuer_spki_hash_len: {}", e))
        })?;
        let mut issuer_spki_hash = vec![0; issuer_spki_hash_len as usize];
        reader.read_exact(&mut issuer_spki_hash).map_err(|e| {
            SecurityStateError::from(format!("error reading stash issuer_spki_hash: {}", e))
        })?;
        let serials = dest.entry(issuer_spki_hash).or_insert(HashSet::new());
        for _ in 0..num_serials {
            let serial_len = reader.read_u8().map_err(|e| {
                SecurityStateError::from(format!("error reading stash serial_len: {}", e))
            })?;
            let mut serial = vec![0; serial_len as usize];
            reader.read_exact(&mut serial).map_err(|e| {
                SecurityStateError::from(format!("error reading stash serial: {}", e))
            })?;
            let _ = serials.insert(serial);
        }
    }
    Ok(())
}

// This is a helper struct that implements the task that asynchronously reads the CRLite stash on a
// background thread.
struct BackgroundReadStashTask {
    profile_path: PathBuf,
    security_state: Arc<RwLock<SecurityState>>,
}

impl BackgroundReadStashTask {
    fn new(
        profile_path: PathBuf,
        security_state: &Arc<RwLock<SecurityState>>,
    ) -> BackgroundReadStashTask {
        BackgroundReadStashTask {
            profile_path,
            security_state: Arc::clone(security_state),
        }
    }
}

impl Task for BackgroundReadStashTask {
    fn run(&self) {
        let mut path = match get_store_path(&self.profile_path) {
            Ok(path) => path,
            Err(e) => {
                error!("error getting security_state path: {}", e.message);
                return;
            }
        };
        path.push("crlite.stash");
        // Before we've downloaded any stashes, this file won't exist.
        if !path.exists() {
            return;
        }
        let stash_file = match File::open(path) {
            Ok(file) => file,
            Err(e) => {
                error!("error opening stash file: {}", e);
                return;
            }
        };
        let mut stash_reader = BufReader::new(stash_file);
        let mut crlite_stash = HashMap::new();
        match load_crlite_stash_from_reader_into_map(&mut stash_reader, &mut crlite_stash) {
            Ok(()) => {}
            Err(e) => {
                error!("error loading crlite stash: {}", e.message);
                return;
            }
        }
        let mut ss = match self.security_state.write() {
            Ok(ss) => ss,
            Err(_) => return,
        };
        match ss.crlite_stash.replace(crlite_stash) {
            Some(_) => {
                error!("replacing existing crlite stash when reading for the first time?");
                return;
            }
            None => {}
        }
    }

    fn done(&self) -> Result<(), nsresult> {
        Ok(())
    }
}

fn do_construct_cert_storage(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> Result<(), nserror::nsresult> {
    let path_buf = get_profile_path()?;
    let security_state = Arc::new(RwLock::new(SecurityState::new(path_buf.clone())));
    let cert_storage = CertStorage::allocate(InitCertStorage {
        security_state: security_state.clone(),
        queue: create_background_task_queue(cstr!("cert_storage"))?,
    });
    let memory_reporter = MemoryReporter::allocate(InitMemoryReporter { security_state });

    // Dispatch a task to the background task queue to asynchronously read the CRLite stash file (if
    // present) and load it into cert_storage. This task does not hold the
    // cert_storage.security_state mutex for the majority of its operation, which allows certificate
    // verification threads to query cert_storage without blocking. This is important for
    // performance, but it means that certificate verifications that happen before the task has
    // completed will not have stash information, and thus may not know of revocations that have
    // occurred since the last full CRLite filter was downloaded. As long as the last full filter
    // was downloaded no more than 10 days ago, this is no worse than relying on OCSP responses,
    // which have a maximum validity of 10 days.
    // NB: because the background task queue is serial, this task will complete before other tasks
    // later dispatched to the queue run. This means that other tasks that interact with the stash
    // will do so with the correct set of preconditions.
    let load_crlite_stash_task = Box::new(BackgroundReadStashTask::new(
        path_buf,
        &cert_storage.security_state,
    ));
    let runnable = TaskRunnable::new("LoadCrliteStash", load_crlite_stash_task)?;
    TaskRunnable::dispatch(runnable, cert_storage.queue.coerce())?;

    if let Some(reporter) = memory_reporter.query_interface::<nsIMemoryReporter>() {
        if let Some(reporter_manager) = xpcom::get_service::<nsIMemoryReporterManager>(cstr!(
            "@mozilla.org/memory-reporter-manager;1"
        )) {
            unsafe { reporter_manager.RegisterStrongReporter(&*reporter) };
        }
    }

    unsafe { cert_storage.QueryInterface(iid, result).to_result() }
}

// This is a helper for creating a task that will perform a specific action on a background thread.
struct SecurityStateTask<
    T: Default + VariantType,
    F: FnOnce(&mut SecurityState) -> Result<T, SecurityStateError>,
> {
    callback: AtomicCell<Option<ThreadBoundRefPtr<nsICertStorageCallback>>>,
    security_state: Arc<RwLock<SecurityState>>,
    result: AtomicCell<(nserror::nsresult, T)>,
    task_action: AtomicCell<Option<F>>,
}

impl<T: Default + VariantType, F: FnOnce(&mut SecurityState) -> Result<T, SecurityStateError>>
    SecurityStateTask<T, F>
{
    fn new(
        callback: &nsICertStorageCallback,
        security_state: &Arc<RwLock<SecurityState>>,
        task_action: F,
    ) -> Result<SecurityStateTask<T, F>, nsresult> {
        let mut ss = security_state.write().or(Err(NS_ERROR_FAILURE))?;
        ss.remaining_ops = ss.remaining_ops.wrapping_add(1);

        Ok(SecurityStateTask {
            callback: AtomicCell::new(Some(ThreadBoundRefPtr::new(RefPtr::new(callback)))),
            security_state: Arc::clone(security_state),
            result: AtomicCell::new((NS_ERROR_FAILURE, T::default())),
            task_action: AtomicCell::new(Some(task_action)),
        })
    }
}

impl<T: Default + VariantType, F: FnOnce(&mut SecurityState) -> Result<T, SecurityStateError>> Task
    for SecurityStateTask<T, F>
{
    fn run(&self) {
        let mut ss = match self.security_state.write() {
            Ok(ss) => ss,
            Err(_) => return,
        };
        // this is a no-op if the DB is already open
        if ss.open_db().is_err() {
            return;
        }
        if let Some(task_action) = self.task_action.swap(None) {
            let rv = task_action(&mut ss)
                .and_then(|v| Ok((NS_OK, v)))
                .unwrap_or((NS_ERROR_FAILURE, T::default()));
            self.result.store(rv);
        }
    }

    fn done(&self) -> Result<(), nsresult> {
        let threadbound = self.callback.swap(None).ok_or(NS_ERROR_FAILURE)?;
        let callback = threadbound.get_ref().ok_or(NS_ERROR_FAILURE)?;
        let result = self.result.swap((NS_ERROR_FAILURE, T::default()));
        let variant = result.1.into_variant();
        let nsrv = unsafe { callback.Done(result.0, &*variant) };

        let mut ss = self.security_state.write().or(Err(NS_ERROR_FAILURE))?;
        ss.remaining_ops = ss.remaining_ops.wrapping_sub(1);

        match nsrv {
            NS_OK => Ok(()),
            e => Err(e),
        }
    }
}

#[no_mangle]
pub extern "C" fn cert_storage_constructor(
    iid: *const xpcom::nsIID,
    result: *mut *mut xpcom::reexports::libc::c_void,
) -> nserror::nsresult {
    if !is_main_thread() {
        return NS_ERROR_NOT_SAME_THREAD;
    }
    match do_construct_cert_storage(iid, result) {
        Ok(()) => NS_OK,
        Err(e) => e,
    }
}

macro_rules! try_ns {
    ($e:expr) => {
        match $e {
            Ok(value) => value,
            Err(_) => return NS_ERROR_FAILURE,
        }
    };
    ($e:expr, or continue) => {
        match $e {
            Ok(value) => value,
            Err(err) => {
                error!("{}", err);
                continue;
            }
        }
    };
}

// This macro is a way to ensure the DB has been opened while minimizing lock acquisitions in the
// common (read-only) case. First we acquire a read lock and see if we even need to open the DB. If
// not, we can continue with the read lock we already have. Otherwise, we drop the read lock,
// acquire the write lock, open the DB, drop the write lock, and re-acquire the read lock. While it
// is possible for two or more threads to all come to the conclusion that they need to open the DB,
// this isn't ultimately an issue - `open_db` will exit early if another thread has already done the
// work.
macro_rules! get_security_state {
    ($self:expr) => {{
        let ss_read_only = try_ns!($self.security_state.read());
        if !ss_read_only.db_needs_opening() {
            ss_read_only
        } else {
            drop(ss_read_only);
            {
                let mut ss_write = try_ns!($self.security_state.write());
                try_ns!(ss_write.open_db());
            }
            try_ns!($self.security_state.read())
        }
    }};
}

#[xpcom(implement(nsICertStorage), atomic)]
struct CertStorage {
    security_state: Arc<RwLock<SecurityState>>,
    queue: RefPtr<nsISerialEventTarget>,
}

/// CertStorage implements the nsICertStorage interface. The actual work is done by the
/// SecurityState. To handle any threading issues, we have an atomic-refcounted read/write lock on
/// the one and only SecurityState. So, only one thread can use SecurityState's &mut self functions
/// at a time, while multiple threads can use &self functions simultaneously (as long as there are
/// no threads using an &mut self function). The Arc is to allow for the creation of background
/// tasks that use the SecurityState on the queue owned by CertStorage. This allows us to not block
/// the main thread.
#[allow(non_snake_case)]
impl CertStorage {
    unsafe fn HasPriorData(
        &self,
        data_type: u8,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if callback.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.get_has_prior_data(data_type),
        )));
        let runnable = try_ns!(TaskRunnable::new("HasPriorData", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
        NS_OK
    }

    unsafe fn GetRemainingOperationCount(&self, state: *mut i32) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if state.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let ss = try_ns!(self.security_state.read());
        *state = ss.remaining_ops;
        NS_OK
    }

    unsafe fn SetRevocations(
        &self,
        revocations: *const ThinVec<Option<RefPtr<nsIRevocationState>>>,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if revocations.is_null() || callback.is_null() {
            return NS_ERROR_NULL_POINTER;
        }

        let revocations = &*revocations;
        let mut entries = Vec::with_capacity(revocations.len());

        // By continuing when an nsIRevocationState attribute value is invalid,
        // we prevent errors relating to individual blocklist entries from
        // causing sync to fail. We will accumulate telemetry on these failures
        // in bug 1254099.

        for revocation in revocations.iter().flatten() {
            let mut state: i16 = 0;
            try_ns!(revocation.GetState(&mut state).to_result(), or continue);

            if let Some(revocation) =
                (*revocation).query_interface::<nsIIssuerAndSerialRevocationState>()
            {
                let mut issuer = nsCString::new();
                try_ns!(revocation.GetIssuer(&mut *issuer).to_result(), or continue);

                let mut serial = nsCString::new();
                try_ns!(revocation.GetSerial(&mut *serial).to_result(), or continue);

                entries.push(EncodedSecurityState::new(
                    PREFIX_REV_IS,
                    issuer,
                    serial,
                    state,
                ));
            } else if let Some(revocation) =
                (*revocation).query_interface::<nsISubjectAndPubKeyRevocationState>()
            {
                let mut subject = nsCString::new();
                try_ns!(revocation.GetSubject(&mut *subject).to_result(), or continue);

                let mut pub_key_hash = nsCString::new();
                try_ns!(revocation.GetPubKey(&mut *pub_key_hash).to_result(), or continue);

                entries.push(EncodedSecurityState::new(
                    PREFIX_REV_SPK,
                    subject,
                    pub_key_hash,
                    state,
                ));
            }
        }

        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.set_batch_state(&entries, nsICertStorage::DATA_TYPE_REVOCATION),
        )));
        let runnable = try_ns!(TaskRunnable::new("SetRevocations", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
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
        // can't do so until bug 1406854 is fixed.
        if issuer.is_null() || serial.is_null() || subject.is_null() || pub_key.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        *state = nsICertStorage::STATE_UNSET;
        let ss = get_security_state!(self);
        match ss.get_revocation_state(&*issuer, &*serial, &*subject, &*pub_key) {
            Ok(st) => {
                *state = st;
                NS_OK
            }
            _ => NS_ERROR_FAILURE,
        }
    }

    unsafe fn SetFullCRLiteFilter(
        &self,
        filter: *const ThinVec<u8>,
        enrolled_issuers: *const ThinVec<nsCString>,
        coverage: *const ThinVec<Option<RefPtr<nsICRLiteCoverage>>>,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if filter.is_null()
            || coverage.is_null()
            || callback.is_null()
            || enrolled_issuers.is_null()
        {
            return NS_ERROR_NULL_POINTER;
        }

        let filter_owned = (*filter).to_vec();
        let enrolled_issuers_owned = (*enrolled_issuers).to_vec();

        let coverage = &*coverage;
        let mut coverage_entries = Vec::with_capacity(coverage.len());
        for entry in coverage.iter().flatten() {
            let mut b64_log_id = nsCString::new();
            try_ns!((*entry).GetB64LogID(&mut *b64_log_id).to_result(), or continue);
            let mut min_timestamp: u64 = 0;
            try_ns!((*entry).GetMinTimestamp(&mut min_timestamp).to_result(), or continue);
            let mut max_timestamp: u64 = 0;
            try_ns!((*entry).GetMaxTimestamp(&mut max_timestamp).to_result(), or continue);
            coverage_entries.push((b64_log_id, min_timestamp, max_timestamp));
        }

        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.set_full_crlite_filter(
                filter_owned,
                enrolled_issuers_owned,
                &coverage_entries
            ),
        )));
        let runnable = try_ns!(TaskRunnable::new("SetFullCRLiteFilter", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
        NS_OK
    }

    unsafe fn AddCRLiteStash(
        &self,
        stash: *const ThinVec<u8>,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if stash.is_null() || callback.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let stash_owned = (*stash).to_vec();
        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.add_crlite_stash(stash_owned),
        )));
        let runnable = try_ns!(TaskRunnable::new("AddCRLiteStash", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
        NS_OK
    }

    unsafe fn IsCertRevokedByStash(
        &self,
        issuer_spki: *const ThinVec<u8>,
        serial_number: *const ThinVec<u8>,
        is_revoked: *mut bool,
    ) -> nserror::nsresult {
        if issuer_spki.is_null() || serial_number.is_null() || is_revoked.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let ss = get_security_state!(self);
        *is_revoked = match ss.is_cert_revoked_by_stash(&*issuer_spki, &*serial_number) {
            Ok(is_revoked) => is_revoked,
            Err(_) => return NS_ERROR_FAILURE,
        };
        NS_OK
    }

    unsafe fn GetCRLiteRevocationState(
        &self,
        issuer: *const ThinVec<u8>,
        issuerSPKI: *const ThinVec<u8>,
        serialNumber: *const ThinVec<u8>,
        timestamps: *const ThinVec<Option<RefPtr<nsICRLiteTimestamp>>>,
        state: *mut i16,
    ) -> nserror::nsresult {
        // TODO (bug 1541212): We really want to restrict this to non-main-threads only, but we
        // can't do so until bug 1406854 is fixed.
        if issuer.is_null()
            || issuerSPKI.is_null()
            || serialNumber.is_null()
            || state.is_null()
            || timestamps.is_null()
        {
            return NS_ERROR_NULL_POINTER;
        }
        let timestamps = &*timestamps;
        let mut timestamp_entries = Vec::with_capacity(timestamps.len());
        for timestamp_entry in timestamps.iter().flatten() {
            let mut log_id = ThinVec::with_capacity(32);
            try_ns!(timestamp_entry.GetLogID(&mut log_id).to_result(), or continue);
            let mut timestamp: u64 = 0;
            try_ns!(timestamp_entry.GetTimestamp(&mut timestamp).to_result(), or continue);
            timestamp_entries.push(CRLiteTimestamp { log_id, timestamp });
        }
        let ss = get_security_state!(self);
        *state = ss.get_crlite_revocation_state(
            &*issuer,
            &*issuerSPKI,
            &*serialNumber,
            &timestamp_entries,
        );
        NS_OK
    }

    unsafe fn AddCerts(
        &self,
        certs: *const ThinVec<Option<RefPtr<nsICertInfo>>>,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if certs.is_null() || callback.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let certs = &*certs;
        let mut cert_entries = Vec::with_capacity(certs.len());
        for cert in certs.iter().flatten() {
            let mut der = nsCString::new();
            try_ns!((*cert).GetCert(&mut *der).to_result(), or continue);
            let mut subject = nsCString::new();
            try_ns!((*cert).GetSubject(&mut *subject).to_result(), or continue);
            let mut trust: i16 = 0;
            try_ns!((*cert).GetTrust(&mut trust).to_result(), or continue);
            cert_entries.push((der, subject, trust));
        }
        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.add_certs(&cert_entries),
        )));
        let runnable = try_ns!(TaskRunnable::new("AddCerts", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
        NS_OK
    }

    unsafe fn RemoveCertsByHashes(
        &self,
        hashes: *const ThinVec<nsCString>,
        callback: *const nsICertStorageCallback,
    ) -> nserror::nsresult {
        if !is_main_thread() {
            return NS_ERROR_NOT_SAME_THREAD;
        }
        if hashes.is_null() || callback.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let hashes = (*hashes).to_vec();
        let task = Box::new(try_ns!(SecurityStateTask::new(
            &*callback,
            &self.security_state,
            move |ss| ss.remove_certs_by_hashes(&hashes),
        )));
        let runnable = try_ns!(TaskRunnable::new("RemoveCertsByHashes", task));
        try_ns!(TaskRunnable::dispatch(runnable, self.queue.coerce()));
        NS_OK
    }

    unsafe fn FindCertsBySubject(
        &self,
        subject: *const ThinVec<u8>,
        certs: *mut ThinVec<ThinVec<u8>>,
    ) -> nserror::nsresult {
        // TODO (bug 1541212): We really want to restrict this to non-main-threads only, but we
        // can't do so until bug 1406854 is fixed.
        if subject.is_null() || certs.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        let ss = get_security_state!(self);
        match ss.find_certs_by_subject(&*subject, &mut *certs) {
            Ok(()) => NS_OK,
            Err(_) => NS_ERROR_FAILURE,
        }
    }
}

extern "C" {
    fn cert_storage_malloc_size_of(ptr: *const xpcom::reexports::libc::c_void) -> usize;
}

#[xpcom(implement(nsIMemoryReporter), atomic)]
struct MemoryReporter {
    security_state: Arc<RwLock<SecurityState>>,
}

#[allow(non_snake_case)]
impl MemoryReporter {
    unsafe fn CollectReports(
        &self,
        callback: *const nsIHandleReportCallback,
        data: *const nsISupports,
        _anonymize: bool,
    ) -> nserror::nsresult {
        let ss = try_ns!(self.security_state.read());
        let mut ops = MallocSizeOfOps::new(cert_storage_malloc_size_of, None);
        let size = ss.size_of(&mut ops);
        let callback = match RefPtr::from_raw(callback) {
            Some(ptr) => ptr,
            None => return NS_ERROR_UNEXPECTED,
        };
        // This does the same as MOZ_COLLECT_REPORT
        callback.Callback(
            &nsCStr::new() as &nsACString,
            &nsCStr::from("explicit/cert-storage/storage") as &nsACString,
            nsIMemoryReporter::KIND_HEAP,
            nsIMemoryReporter::UNITS_BYTES,
            size as i64,
            &nsCStr::from("Memory used by certificate storage") as &nsACString,
            data,
        );
        NS_OK
    }
}
