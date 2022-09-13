/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    error::{XULStoreError, XULStoreResult},
    ffi::ProfileChangeObserver,
    make_key, SEPARATOR,
};
use moz_task::is_main_thread;
use nsstring::nsString;
use once_cell::sync::Lazy;
use rkv::backend::{SafeMode, SafeModeDatabase, SafeModeEnvironment};
use rkv::{StoreOptions, Value};
use std::{
    collections::BTreeMap,
    fs::{create_dir_all, remove_file, File},
    path::PathBuf,
    str,
    sync::{Arc, Mutex, RwLock},
};
use xpcom::{
    interfaces::{nsIFile, nsIObserverService, nsIProperties, nsIXULRuntime},
    RefPtr, XpCom,
};

type Manager = rkv::Manager<SafeModeEnvironment>;
type Rkv = rkv::Rkv<SafeModeEnvironment>;
type SingleStore = rkv::SingleStore<SafeModeDatabase>;
type XULStoreCache = BTreeMap<String, BTreeMap<String, BTreeMap<String, String>>>;

pub struct Database {
    pub rkv: Arc<RwLock<Rkv>>,
    pub store: SingleStore,
}

impl Database {
    fn new(rkv: Arc<RwLock<Rkv>>, store: SingleStore) -> Database {
        Database { rkv, store }
    }
}

static PROFILE_DIR: Lazy<Mutex<Option<PathBuf>>> = Lazy::new(|| {
    observe_profile_change();
    Mutex::new(get_profile_dir().ok())
});

pub(crate) static DATA_CACHE: Lazy<Mutex<Option<XULStoreCache>>> =
    Lazy::new(|| Mutex::new(cache_data().ok()));

pub(crate) fn get_database() -> XULStoreResult<Database> {
    let mut manager = Manager::singleton().write()?;
    let xulstore_dir = get_xulstore_dir()?;
    let xulstore_path = xulstore_dir.as_path();
    let rkv = manager.get_or_create(xulstore_path, Rkv::new::<SafeMode>)?;
    let store = rkv.read()?.open_single("db", StoreOptions::create())?;
    Ok(Database::new(rkv, store))
}

pub(crate) fn update_profile_dir() {
    // Failure to update the dir isn't fatal (although it means that we won't
    // persist XULStore data for this session), so we don't return a result.
    // But we use a closure returning a result to enable use of the ? operator.
    (|| -> XULStoreResult<()> {
        {
            let mut profile_dir_guard = PROFILE_DIR.lock()?;
            *profile_dir_guard = get_profile_dir().ok();
        }

        let mut cache_guard = DATA_CACHE.lock()?;
        *cache_guard = cache_data().ok();

        Ok(())
    })()
    .unwrap_or_else(|err| error!("error updating profile dir: {}", err));
}

fn get_profile_dir() -> XULStoreResult<PathBuf> {
    // We can't use getter_addrefs() here because get_DirectoryService()
    // returns its nsIProperties interface, and its Get() method returns
    // a directory via its nsQIResult out param, which gets translated to
    // a `*mut *mut libc::c_void` in Rust, whereas getter_addrefs() expects
    // a closure with a `*mut *const T` parameter.

    let dir_svc: RefPtr<nsIProperties> =
        xpcom::components::Directory::service().map_err(|_| XULStoreError::Unavailable)?;
    let mut profile_dir = xpcom::GetterAddrefs::<nsIFile>::new();
    unsafe {
        dir_svc
            .Get(
                cstr!("ProfD").as_ptr(),
                &nsIFile::IID,
                profile_dir.void_ptr(),
            )
            .to_result()
            .or_else(|_| {
                dir_svc
                    .Get(
                        cstr!("ProfDS").as_ptr(),
                        &nsIFile::IID,
                        profile_dir.void_ptr(),
                    )
                    .to_result()
            })?;
    }
    let profile_dir = profile_dir.refptr().ok_or(XULStoreError::Unavailable)?;

    let mut profile_path = nsString::new();
    unsafe {
        profile_dir.GetPath(&mut *profile_path).to_result()?;
    }

    let path = String::from_utf16(&profile_path[..])?;
    Ok(PathBuf::from(&path))
}

fn get_xulstore_dir() -> XULStoreResult<PathBuf> {
    let mut xulstore_dir = PROFILE_DIR
        .lock()?
        .clone()
        .ok_or(XULStoreError::Unavailable)?;
    xulstore_dir.push("xulstore");

    create_dir_all(xulstore_dir.clone())?;

    Ok(xulstore_dir)
}

fn observe_profile_change() {
    assert!(is_main_thread());

    // Failure to observe the change isn't fatal (although it means we won't
    // persist XULStore data for this session), so we don't return a result.
    // But we use a closure returning a result to enable use of the ? operator.
    (|| -> XULStoreResult<()> {
        // Observe profile changes so we can update this directory accordingly.
        let obs_svc: RefPtr<nsIObserverService> =
            xpcom::components::Observer::service().map_err(|_| XULStoreError::Unavailable)?;
        let observer = ProfileChangeObserver::new();
        unsafe {
            obs_svc
                .AddObserver(
                    observer.coerce(),
                    cstr!("profile-after-change").as_ptr(),
                    false,
                )
                .to_result()?
        };
        Ok(())
    })()
    .unwrap_or_else(|err| error!("error observing profile change: {}", err));
}

fn in_safe_mode() -> XULStoreResult<bool> {
    let xul_runtime: RefPtr<nsIXULRuntime> =
        xpcom::components::XULRuntime::service().map_err(|_| XULStoreError::Unavailable)?;
    let mut in_safe_mode = false;
    unsafe {
        xul_runtime.GetInSafeMode(&mut in_safe_mode).to_result()?;
    }
    Ok(in_safe_mode)
}

fn cache_data() -> XULStoreResult<XULStoreCache> {
    let db = get_database()?;
    maybe_migrate_data(&db, db.store);

    let mut all = XULStoreCache::default();
    if in_safe_mode()? {
        return Ok(all);
    }

    let env = db.rkv.read()?;
    let reader = env.read()?;
    let iterator = db.store.iter_start(&reader)?;

    for result in iterator {
        let (key, value): (&str, String) = match result {
            Ok((key, value)) => match (str::from_utf8(&key), unwrap_value(&value)) {
                (Ok(key), Ok(value)) => (key, value),
                (Err(err), _) => return Err(err.into()),
                (_, Err(err)) => return Err(err),
            },
            Err(err) => return Err(err.into()),
        };

        let parts = key.split(SEPARATOR).collect::<Vec<&str>>();
        if parts.len() != 3 {
            return Err(XULStoreError::UnexpectedKey(key.to_string()));
        }
        let (doc, id, attr) = (
            parts[0].to_string(),
            parts[1].to_string(),
            parts[2].to_string(),
        );

        all.entry(doc)
            .or_default()
            .entry(id)
            .or_default()
            .entry(attr)
            .or_insert(value);
    }

    Ok(all)
}

fn maybe_migrate_data(db: &Database, store: SingleStore) {
    // Failure to migrate data isn't fatal, so we don't return a result.
    // But we use a closure returning a result to enable use of the ? operator.
    (|| -> XULStoreResult<()> {
        let mut old_datastore = PROFILE_DIR
            .lock()?
            .clone()
            .ok_or(XULStoreError::Unavailable)?;
        old_datastore.push("xulstore.json");
        if !old_datastore.exists() {
            debug!("old datastore doesn't exist: {:?}", old_datastore);
            return Ok(());
        }

        let file = File::open(old_datastore.clone())?;
        let json: XULStoreCache = serde_json::from_reader(file)?;

        let env = db.rkv.read()?;
        let mut writer = env.write()?;

        for (doc, ids) in json {
            for (id, attrs) in ids {
                for (attr, value) in attrs {
                    let key = make_key(&doc, &id, &attr);
                    store.put(&mut writer, &key, &Value::Str(&value))?;
                }
            }
        }

        writer.commit()?;

        remove_file(old_datastore)?;

        Ok(())
    })()
    .unwrap_or_else(|err| error!("error migrating data: {}", err));
}

fn unwrap_value(value: &Value) -> XULStoreResult<String> {
    match value {
        Value::Str(val) => Ok(val.to_string()),

        // This should never happen, but it could happen in theory
        // if someone writes a different kind of value into the store
        // using a more general API (kvstore, rkv, LMDB).
        _ => Err(XULStoreError::UnexpectedValue),
    }
}
