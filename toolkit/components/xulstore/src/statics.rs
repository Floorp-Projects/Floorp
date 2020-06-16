/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    error::{XULStoreError, XULStoreResult},
    ffi::ProfileChangeObserver,
    make_key, SEPARATOR,
};
use lmdb::Error as LmdbError;
use moz_task::is_main_thread;
use nsstring::nsString;
use once_cell::sync::Lazy;
use rkv::{migrate::Migrator, Rkv, SingleStore, StoreError, StoreOptions, Value};
use std::{
    collections::BTreeMap,
    fs::{copy, create_dir_all, remove_file, File},
    path::PathBuf,
    str,
    sync::Mutex,
};
use tempfile::tempdir;
use xpcom::{interfaces::nsIFile, XpCom};

type XULStoreCache = BTreeMap<String, BTreeMap<String, BTreeMap<String, String>>>;

pub struct Database {
    pub env: Rkv,
    pub store: SingleStore,
}

impl Database {
    fn new(env: Rkv, store: SingleStore) -> Database {
        Database { env, store }
    }
}

static PROFILE_DIR: Lazy<Mutex<Option<PathBuf>>> = Lazy::new(|| {
    observe_profile_change();
    Mutex::new(get_profile_dir().ok())
});

pub(crate) static DATA_CACHE: Lazy<Mutex<Option<XULStoreCache>>> =
    Lazy::new(|| Mutex::new(cache_data().ok()));

pub(crate) fn get_database() -> XULStoreResult<Database> {
    let xulstore_dir = get_xulstore_dir()?;
    let xulstore_path = xulstore_dir.as_path();

    let env = match Rkv::new(xulstore_path) {
        Ok(env) => Ok(env),
        Err(StoreError::LmdbError(LmdbError::Invalid)) => {
            let temp_env = tempdir()?;
            let mut migrator = Migrator::new(&xulstore_path)?;
            migrator.migrate(temp_env.path())?;
            copy(
                temp_env.path().join("data.mdb"),
                xulstore_path.join("data.mdb"),
            )?;
            copy(
                temp_env.path().join("lock.mdb"),
                xulstore_path.join("lock.mdb"),
            )?;
            Rkv::new(xulstore_path)
        }
        Err(err) => Err(err),
    }?;

    let store = env.open_single("db", StoreOptions::create())?;

    Ok(Database::new(env, store))
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

    let dir_svc = xpcom::services::get_DirectoryService().ok_or(XULStoreError::Unavailable)?;
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
        let obs_svc = xpcom::services::get_ObserverService().ok_or(XULStoreError::Unavailable)?;
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
    let xul_runtime = xpcom::services::get_XULRuntime().ok_or(XULStoreError::Unavailable)?;
    let mut in_safe_mode = false;
    unsafe {
        xul_runtime.GetInSafeMode(&mut in_safe_mode).to_result()?;
    }
    Ok(in_safe_mode)
}

fn cache_data() -> XULStoreResult<XULStoreCache> {
    let db = get_database()?;
    maybe_migrate_data(&db.env, db.store);

    let mut all = XULStoreCache::default();
    if in_safe_mode()? {
        return Ok(all);
    }

    let reader = db.env.read()?;
    let iterator = db.store.iter_start(&reader)?;

    for result in iterator {
        let (key, value): (&str, String) = match result {
            Ok((key, value)) => {
                assert!(value.is_some(), "iterated key has value");
                match (str::from_utf8(&key), unwrap_value(&value)) {
                    (Ok(key), Ok(value)) => (key, value),
                    (Err(err), _) => return Err(err.into()),
                    (_, Err(err)) => return Err(err),
                }
            }
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

fn maybe_migrate_data(env: &Rkv, store: SingleStore) {
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

fn unwrap_value(value: &Option<Value>) -> XULStoreResult<String> {
    match value {
        Some(Value::Str(val)) => Ok(val.to_string()),

        // Per the XULStore API, return an empty string if the value
        // isn't found.
        None => Ok(String::new()),

        // This should never happen, but it could happen in theory
        // if someone writes a different kind of value into the store
        // using a more general API (kvstore, rkv, LMDB).
        Some(_) => Err(XULStoreError::UnexpectedValue),
    }
}
