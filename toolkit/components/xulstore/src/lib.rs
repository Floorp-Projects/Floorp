/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate crossbeam_utils;
#[macro_use]
extern crate cstr;
#[macro_use]
extern crate failure;
extern crate libc;
extern crate lmdb;
#[macro_use]
extern crate log;
extern crate moz_task;
extern crate nserror;
extern crate nsstring;
extern crate once_cell;
extern crate rkv;
extern crate serde_json;
extern crate tempfile;
#[macro_use]
extern crate xpcom;

mod error;
mod ffi;
mod iter;
mod persist;
mod statics;

use crate::{
    error::{XULStoreError, XULStoreResult},
    iter::XULStoreIterator,
    persist::{flush_writes, persist},
    statics::DATA_CACHE,
};
use nsstring::nsAString;
use std::collections::btree_map::Entry;
use std::fmt::Display;

const SEPARATOR: char = '\u{0009}';

pub(crate) fn make_key(doc: &impl Display, id: &impl Display, attr: &impl Display) -> String {
    format!("{}{}{}{}{}", doc, SEPARATOR, id, SEPARATOR, attr)
}

pub(crate) fn set_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
    value: &nsAString,
) -> XULStoreResult<()> {
    debug!("XULStore set value: {} {} {} {}", doc, id, attr, value);

    // bug 319846 -- don't save really long attributes or values.
    if id.len() > 512 || attr.len() > 512 {
        return Err(XULStoreError::IdAttrNameTooLong);
    }

    let value = if value.len() > 4096 {
        warn!("XULStore: truncating long attribute value");
        String::from_utf16(&value[0..4096])?
    } else {
        String::from_utf16(value)?
    };

    let mut cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_mut() {
        Some(data) => data,
        None => return Ok(()),
    };
    data.entry(doc.to_string())
        .or_default()
        .entry(id.to_string())
        .or_default()
        .insert(attr.to_string(), value.clone());

    persist(make_key(doc, id, attr), Some(value))?;

    Ok(())
}

pub(crate) fn has_value(doc: &nsAString, id: &nsAString, attr: &nsAString) -> XULStoreResult<bool> {
    debug!("XULStore has value: {} {} {}", doc, id, attr);

    let cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_ref() {
        Some(data) => data,
        None => return Ok(false),
    };

    match data.get(&doc.to_string()) {
        Some(ids) => match ids.get(&id.to_string()) {
            Some(attrs) => Ok(attrs.contains_key(&attr.to_string())),
            None => Ok(false),
        },
        None => Ok(false),
    }
}

pub(crate) fn get_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
) -> XULStoreResult<String> {
    debug!("XULStore get value {} {} {}", doc, id, attr);

    let cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_ref() {
        Some(data) => data,
        None => return Ok(String::new()),
    };

    match data.get(&doc.to_string()) {
        Some(ids) => match ids.get(&id.to_string()) {
            Some(attrs) => match attrs.get(&attr.to_string()) {
                Some(value) => Ok(value.clone()),
                None => Ok(String::new()),
            },
            None => Ok(String::new()),
        },
        None => Ok(String::new()),
    }
}

pub(crate) fn remove_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
) -> XULStoreResult<()> {
    debug!("XULStore remove value {} {} {}", doc, id, attr);

    let mut cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_mut() {
        Some(data) => data,
        None => return Ok(()),
    };

    let mut ids_empty = false;
    if let Some(ids) = data.get_mut(&doc.to_string()) {
        let mut attrs_empty = false;
        if let Some(attrs) = ids.get_mut(&id.to_string()) {
            attrs.remove(&attr.to_string());
            if attrs.is_empty() {
                attrs_empty = true;
            }
        }
        if attrs_empty {
            ids.remove(&id.to_string());
            if ids.is_empty() {
                ids_empty = true;
            }
        }
    };
    if ids_empty {
        data.remove(&doc.to_string());
    }

    persist(make_key(doc, id, attr), None)?;

    Ok(())
}

pub(crate) fn remove_document(doc: &nsAString) -> XULStoreResult<()> {
    debug!("XULStore remove document {}", doc);

    let mut cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_mut() {
        Some(data) => data,
        None => return Ok(()),
    };

    if let Entry::Occupied(entry) = data.entry(doc.to_string()) {
        for (id, attrs) in entry.get() {
            for attr in attrs.keys() {
                persist(make_key(entry.key(), id, attr), None)?;
            }
        }
        entry.remove_entry();
    }

    Ok(())
}

pub(crate) fn get_ids(doc: &nsAString) -> XULStoreResult<XULStoreIterator> {
    debug!("XULStore get IDs for {}", doc);

    let cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_ref() {
        Some(data) => data,
        None => return Ok(XULStoreIterator::new(vec![].into_iter())),
    };

    match data.get(&doc.to_string()) {
        Some(ids) => {
            let ids: Vec<String> = ids.keys().cloned().collect();
            Ok(XULStoreIterator::new(ids.into_iter()))
        }
        None => Ok(XULStoreIterator::new(vec![].into_iter())),
    }
}

pub(crate) fn get_attrs(doc: &nsAString, id: &nsAString) -> XULStoreResult<XULStoreIterator> {
    debug!("XULStore get attrs for doc, ID: {} {}", doc, id);

    let cache_guard = DATA_CACHE.lock()?;
    let data = match cache_guard.as_ref() {
        Some(data) => data,
        None => return Ok(XULStoreIterator::new(vec![].into_iter())),
    };

    match data.get(&doc.to_string()) {
        Some(ids) => match ids.get(&id.to_string()) {
            Some(attrs) => {
                let attrs: Vec<String> = attrs.keys().cloned().collect();
                Ok(XULStoreIterator::new(attrs.into_iter()))
            }
            None => Ok(XULStoreIterator::new(vec![].into_iter())),
        },
        None => Ok(XULStoreIterator::new(vec![].into_iter())),
    }
}

pub(crate) fn shutdown() -> XULStoreResult<()> {
    flush_writes()
}
