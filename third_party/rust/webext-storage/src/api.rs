/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use rusqlite::{Connection, Transaction};
use serde::{ser::SerializeMap, Serialize, Serializer};

use serde_json::{Map, Value as JsonValue};
use sql_support::{self, ConnExt};

// These constants are defined by the chrome.storage.sync spec.
const QUOTA_BYTES: usize = 102_400;
const QUOTA_BYTES_PER_ITEM: usize = 8_192;
const MAX_ITEMS: usize = 512;
// Note there are also constants for "operations per minute" etc, which aren't
// enforced here.

type JsonMap = Map<String, JsonValue>;

fn get_from_db(conn: &Connection, ext_id: &str) -> Result<Option<JsonMap>> {
    Ok(
        match conn.try_query_one::<String>(
            "SELECT data FROM storage_sync_data
             WHERE ext_id = :ext_id",
            &[(":ext_id", &ext_id)],
            true,
        )? {
            Some(s) => match serde_json::from_str(&s)? {
                JsonValue::Object(m) => Some(m),
                // we could panic here as it's theoretically impossible, but we
                // might as well treat it as not existing...
                _ => None,
            },
            None => None,
        },
    )
}

fn save_to_db(tx: &Transaction<'_>, ext_id: &str, val: &JsonValue) -> Result<()> {
    // The quota is enforced on the byte count, which is what .len() returns.
    let sval = val.to_string();
    if sval.len() > QUOTA_BYTES {
        return Err(ErrorKind::QuotaError(QuotaReason::TotalBytes).into());
    }
    // XXX - sync support will need to do the change_counter thing here.
    tx.execute_named(
        "INSERT OR REPLACE INTO storage_sync_data(ext_id, data)
            VALUES (:ext_id, :data)",
        &[(":ext_id", &ext_id), (":data", &sval)],
    )?;
    Ok(())
}

fn remove_from_db(tx: &Transaction<'_>, ext_id: &str) -> Result<()> {
    // XXX - sync support will need to do the tombstone thing here.
    tx.execute_named(
        "DELETE FROM storage_sync_data
        WHERE ext_id = :ext_id",
        &[(":ext_id", &ext_id)],
    )?;
    Ok(())
}

// This is a "helper struct" for the callback part of the chrome.storage spec,
// but shaped in a way to make it more convenient from the rust side of the
// world. The strings are all json, we keeping them as strings here makes
// various things easier and avoid a round-trip to/from json/string.
#[derive(Debug, Clone, PartialEq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StorageValueChange {
    #[serde(skip_serializing)]
    key: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    old_value: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    new_value: Option<String>,
}

// This is, largely, a helper so that this serializes correctly as per the
// chrome.storage.sync spec. If not for custom serialization it should just
// be a plain vec
#[derive(Debug, Clone, PartialEq)]
pub struct StorageChanges {
    changes: Vec<StorageValueChange>,
}

impl StorageChanges {
    fn new() -> Self {
        Self {
            changes: Vec::new(),
        }
    }

    fn with_capacity(n: usize) -> Self {
        Self {
            changes: Vec::with_capacity(n),
        }
    }

    fn is_empty(&self) -> bool {
        self.changes.is_empty()
    }

    fn push(&mut self, change: StorageValueChange) {
        self.changes.push(change)
    }
}

// and it serializes as a map.
impl Serialize for StorageChanges {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(self.changes.len()))?;
        for change in &self.changes {
            map.serialize_entry(&change.key, change)?;
        }
        map.end()
    }
}

/// The implementation of `storage[.sync].set()`. On success this returns the
/// StorageChanges defined by the chrome API - it's assumed the caller will
/// arrange to deliver this to observers as defined in that API.
pub fn set(tx: &Transaction<'_>, ext_id: &str, val: JsonValue) -> Result<StorageChanges> {
    let val_map = match val {
        JsonValue::Object(m) => m,
        // Not clear what the error semantics should be yet. For now, pretend an empty map.
        _ => Map::new(),
    };

    let mut current = get_from_db(tx, ext_id)?.unwrap_or_default();

    let mut changes = StorageChanges::with_capacity(val_map.len());

    // iterate over the value we are adding/updating.
    for (k, v) in val_map.into_iter() {
        let old_value = current.remove(&k);
        if current.len() >= MAX_ITEMS {
            return Err(ErrorKind::QuotaError(QuotaReason::MaxItems).into());
        }
        // Setup the change entry for this key, and we can leverage it to check
        // for the quota.
        let new_value_s = v.to_string();
        // Reading the chrome docs literally re the quota, the length of the key
        // is just the string len, but the value is the json val, as bytes
        if k.len() + new_value_s.len() >= QUOTA_BYTES_PER_ITEM {
            return Err(ErrorKind::QuotaError(QuotaReason::ItemBytes).into());
        }
        let change = StorageValueChange {
            key: k.clone(),
            old_value: old_value.map(|ov| ov.to_string()),
            new_value: Some(new_value_s),
        };
        changes.push(change);
        current.insert(k, v);
    }

    save_to_db(tx, ext_id, &JsonValue::Object(current))?;
    Ok(changes)
}

// A helper which takes a param indicating what keys should be returned and
// converts that to a vec of real strings. Also returns "default" values to
// be used if no item exists for that key.
fn get_keys(keys: JsonValue) -> Vec<(String, Option<JsonValue>)> {
    match keys {
        JsonValue::String(s) => vec![(s, None)],
        JsonValue::Array(keys) => {
            // because nothing with json is ever simple, each key may not be
            // a string. We ignore any which aren't.
            keys.iter()
                .filter_map(|v| v.as_str().map(|s| (s.to_string(), None)))
                .collect()
        }
        JsonValue::Object(m) => m.into_iter().map(|(k, d)| (k, Some(d))).collect(),
        _ => vec![],
    }
}

/// The implementation of `storage[.sync].get()` - on success this always
/// returns a Json object.
pub fn get(conn: &Connection, ext_id: &str, keys: JsonValue) -> Result<JsonValue> {
    // key is optional, or string or array of string or object keys
    let maybe_existing = get_from_db(conn, ext_id)?;
    let mut existing = match maybe_existing {
        None => return Ok(JsonValue::Object(Map::new())),
        Some(v) => v,
    };
    // take the quick path for null, where we just return the entire object.
    if keys.is_null() {
        return Ok(JsonValue::Object(existing));
    }
    // OK, so we need to build a list of keys to get.
    let keys_and_defaults = get_keys(keys);
    let mut result = Map::with_capacity(keys_and_defaults.len());
    for (key, maybe_default) in keys_and_defaults {
        // XXX - If a key is requested that doesn't exist, we have 2 options:
        // (1) have the key in the result with the value null, or (2) the key
        // simply doesn't exist in the result. We assume (2), but should verify
        // that's what chrome does.
        if let Some(v) = existing.remove(&key) {
            result.insert(key, v);
        } else if let Some(def) = maybe_default {
            result.insert(key, def);
        }
    }
    Ok(JsonValue::Object(result))
}

/// The implementation of `storage[.sync].remove()`. On success this returns the
/// StorageChanges defined by the chrome API - it's assumed the caller will
/// arrange to deliver this to observers as defined in that API.
pub fn remove(tx: &Transaction<'_>, ext_id: &str, keys: JsonValue) -> Result<StorageChanges> {
    let mut existing = match get_from_db(tx, ext_id)? {
        None => return Ok(StorageChanges::new()),
        Some(v) => v,
    };

    let keys_and_defs = get_keys(keys);

    let mut result = StorageChanges::with_capacity(keys_and_defs.len());
    for (key, _) in keys_and_defs {
        if let Some(v) = existing.remove(&key) {
            result.push(StorageValueChange {
                key,
                old_value: Some(v.to_string()),
                new_value: None,
            });
        }
    }
    if !result.is_empty() {
        save_to_db(tx, ext_id, &JsonValue::Object(existing))?;
    }
    Ok(result)
}

/// The implementation of `storage[.sync].clear()`. On success this returns the
/// StorageChanges defined by the chrome API - it's assumed the caller will
/// arrange to deliver this to observers as defined in that API.
pub fn clear(tx: &Transaction<'_>, ext_id: &str) -> Result<StorageChanges> {
    let existing = match get_from_db(tx, ext_id)? {
        None => return Ok(StorageChanges::new()),
        Some(v) => v,
    };
    let mut result = StorageChanges::with_capacity(existing.len());
    for (key, val) in existing.into_iter() {
        result.push(StorageValueChange {
            key: key.to_string(),
            new_value: None,
            old_value: Some(val.to_string()),
        });
    }
    remove_from_db(tx, ext_id)?;
    Ok(result)
}

// TODO - get_bytes_in_use()

#[cfg(test)]
mod tests {
    use super::*;
    use crate::db::test::new_mem_db;
    use serde_json::json;

    #[test]
    fn test_serialize_storage_changes() -> Result<()> {
        let c = StorageChanges {
            changes: vec![StorageValueChange {
                key: "key".to_string(),
                old_value: Some("old".to_string()),
                new_value: None,
            }],
        };
        assert_eq!(serde_json::to_string(&c)?, r#"{"key":{"oldValue":"old"}}"#);
        Ok(())
    }

    fn make_changes(changes: &[(&str, Option<JsonValue>, Option<JsonValue>)]) -> StorageChanges {
        let mut r = StorageChanges::with_capacity(changes.len());
        for (name, old_value, new_value) in changes {
            r.push(StorageValueChange {
                key: (*name).to_string(),
                old_value: old_value.as_ref().map(|v| v.to_string()),
                new_value: new_value.as_ref().map(|v| v.to_string()),
            });
        }
        r
    }

    #[test]
    fn test_simple() -> Result<()> {
        let ext_id = "x";
        let db = new_mem_db();
        let mut conn = db.writer.lock().unwrap();
        let tx = conn.transaction()?;

        // an empty store.
        for q in vec![
            JsonValue::Null,
            json!("foo"),
            json!(["foo"]),
            json!({ "foo": null }),
            json!({"foo": "default"}),
        ]
        .into_iter()
        {
            assert_eq!(get(&tx, &ext_id, q)?, json!({}));
        }

        // Single item in the store.
        set(&tx, &ext_id, json!({"foo": "bar" }))?;
        for q in vec![
            JsonValue::Null,
            json!("foo"),
            json!(["foo"]),
            json!({ "foo": null }),
            json!({"foo": "default"}),
        ]
        .into_iter()
        {
            assert_eq!(get(&tx, &ext_id, q)?, json!({"foo": "bar" }));
        }

        // more complex stuff, including changes checking.
        assert_eq!(
            set(&tx, &ext_id, json!({"foo": "new", "other": "also new" }))?,
            make_changes(&[
                ("foo", Some(json!("bar")), Some(json!("new"))),
                ("other", None, Some(json!("also new")))
            ])
        );
        assert_eq!(
            get(&tx, &ext_id, JsonValue::Null)?,
            json!({"foo": "new", "other": "also new"})
        );
        assert_eq!(get(&tx, &ext_id, json!("foo"))?, json!({"foo": "new"}));
        assert_eq!(
            get(&tx, &ext_id, json!(["foo", "other"]))?,
            json!({"foo": "new", "other": "also new"})
        );
        assert_eq!(
            get(&tx, &ext_id, json!({"foo": null, "default": "yo"}))?,
            json!({"foo": "new", "default": "yo"})
        );

        assert_eq!(
            remove(&tx, &ext_id, json!("foo"))?,
            make_changes(&[("foo", Some(json!("new")), None)]),
        );
        // XXX - other variants.

        assert_eq!(
            clear(&tx, &ext_id)?,
            make_changes(&[("other", Some(json!("also new")), None)]),
        );
        assert_eq!(get(&tx, &ext_id, JsonValue::Null)?, json!({}));

        Ok(())
    }

    #[test]
    fn test_check_get_impl() -> Result<()> {
        // This is a port of checkGetImpl in test_ext_storage.js in Desktop.
        let ext_id = "x";
        let db = new_mem_db();
        let mut conn = db.writer.lock().unwrap();
        let tx = conn.transaction()?;

        let prop = "test-prop";
        let value = "test-value";

        set(&tx, ext_id, json!({ prop: value }))?;

        // this is the checkGetImpl part!
        let mut data = get(&tx, &ext_id, json!(null))?;
        assert_eq!(value, json!(data[prop]), "null getter worked for {}", prop);

        data = get(&tx, &ext_id, json!(prop))?;
        assert_eq!(
            value,
            json!(data[prop]),
            "string getter worked for {}",
            prop
        );
        assert_eq!(
            data.as_object().unwrap().len(),
            1,
            "string getter should return an object with a single property"
        );

        data = get(&tx, &ext_id, json!([prop]))?;
        assert_eq!(value, json!(data[prop]), "array getter worked for {}", prop);
        assert_eq!(
            data.as_object().unwrap().len(),
            1,
            "array getter with a single key should return an object with a single property"
        );

        // checkGetImpl() uses `{ [prop]: undefined }` - but json!() can't do that :(
        // Hopefully it's just testing a simple object, so we use `{ prop: null }`
        data = get(&tx, &ext_id, json!({ prop: null }))?;
        assert_eq!(
            value,
            json!(data[prop]),
            "object getter worked for {}",
            prop
        );
        assert_eq!(
            data.as_object().unwrap().len(),
            1,
            "object getter with a single key should return an object with a single property"
        );

        Ok(())
    }

    #[test]
    fn test_bug_1621162() -> Result<()> {
        // apparently Firefox, unlike Chrome, will not optimize the changes.
        // See bug 1621162 for more!
        let db = new_mem_db();
        let mut conn = db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let ext_id = "xyz";

        set(&tx, &ext_id, json!({"foo": "bar" }))?;

        assert_eq!(
            set(&tx, &ext_id, json!({"foo": "bar" }))?,
            make_changes(&[("foo", Some(json!("bar")), Some(json!("bar")))]),
        );
        Ok(())
    }

    #[test]
    fn test_quota_maxitems() -> Result<()> {
        let db = new_mem_db();
        let mut conn = db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let ext_id = "xyz";
        for i in 1..MAX_ITEMS + 1 {
            set(
                &tx,
                &ext_id,
                json!({ format!("key-{}", i): format!("value-{}", i) }),
            )?;
        }
        let e = set(&tx, &ext_id, json!({"another": "another"})).unwrap_err();
        match e.kind() {
            ErrorKind::QuotaError(QuotaReason::MaxItems) => {}
            _ => panic!("unexpected error type"),
        };
        Ok(())
    }

    #[test]
    fn test_quota_bytesperitem() -> Result<()> {
        let db = new_mem_db();
        let mut conn = db.writer.lock().unwrap();
        let tx = conn.transaction()?;
        let ext_id = "xyz";
        // A string 5 bytes less than the max. This should be counted as being
        // 3 bytes less than the max as the quotes are counted.
        let val = "x".repeat(QUOTA_BYTES_PER_ITEM - 5);

        // Key length doesn't push it over.
        set(&tx, &ext_id, json!({ "x": val }))?;

        // Key length does push it over.
        let e = set(&tx, &ext_id, json!({ "xxxx": val })).unwrap_err();
        match e.kind() {
            ErrorKind::QuotaError(QuotaReason::ItemBytes) => {}
            _ => panic!("unexpected error type"),
        };
        Ok(())
    }
}
