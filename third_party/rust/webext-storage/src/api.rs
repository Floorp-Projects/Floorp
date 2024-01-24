/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use rusqlite::{Connection, Transaction};
use serde::{ser::SerializeMap, Serialize, Serializer};

use serde_json::{Map, Value as JsonValue};
use sql_support::{self, ConnExt};

// These constants are defined by the chrome.storage.sync spec. We export them
// publicly from this module, then from the crate, so they wind up in the
// clients.
// Note the limits for `chrome.storage.sync` and `chrome.storage.local` are
// different, and these are from `.sync` - we'll have work to do if we end up
// wanting this to be used for `.local` too!
pub const SYNC_QUOTA_BYTES: usize = 102_400;
pub const SYNC_QUOTA_BYTES_PER_ITEM: usize = 8_192;
pub const SYNC_MAX_ITEMS: usize = 512;
// Note there are also constants for "operations per minute" etc, which aren't
// enforced here.

type JsonMap = Map<String, JsonValue>;

enum StorageChangeOp {
    Clear,
    Set(JsonValue),
    SetWithoutQuota(JsonValue),
}

fn get_from_db(conn: &Connection, ext_id: &str) -> Result<Option<JsonMap>> {
    Ok(
        match conn.try_query_one::<String, _>(
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

fn save_to_db(tx: &Transaction<'_>, ext_id: &str, val: &StorageChangeOp) -> Result<()> {
    // This function also handles removals. Either an empty map or explicit null
    // is a removal. If there's a mirror record for this extension ID, then we
    // must leave a tombstone behind for syncing.
    let is_delete = match val {
        StorageChangeOp::Clear => true,
        StorageChangeOp::Set(JsonValue::Object(v)) => v.is_empty(),
        StorageChangeOp::SetWithoutQuota(JsonValue::Object(v)) => v.is_empty(),
        _ => false,
    };
    if is_delete {
        let in_mirror = tx
            .try_query_one(
                "SELECT EXISTS(SELECT 1 FROM storage_sync_mirror WHERE ext_id = :ext_id);",
                rusqlite::named_params! {
                    ":ext_id": ext_id,
                },
                true,
            )?
            .unwrap_or_default();
        if in_mirror {
            log::trace!("saving data for '{}': leaving a tombstone", ext_id);
            tx.execute_cached(
                "
                INSERT INTO storage_sync_data(ext_id, data, sync_change_counter)
                VALUES (:ext_id, NULL, 1)
                ON CONFLICT (ext_id) DO UPDATE
                SET data = NULL, sync_change_counter = sync_change_counter + 1",
                rusqlite::named_params! {
                    ":ext_id": ext_id,
                },
            )?;
        } else {
            log::trace!("saving data for '{}': removing the row", ext_id);
            tx.execute_cached(
                "
                DELETE FROM storage_sync_data WHERE ext_id = :ext_id",
                rusqlite::named_params! {
                    ":ext_id": ext_id,
                },
            )?;
        }
    } else {
        // Convert to bytes so we can enforce the quota if necessary.
        let sval = match val {
            StorageChangeOp::Set(v) => {
                let sv = v.to_string();
                if sv.len() > SYNC_QUOTA_BYTES {
                    return Err(Error::QuotaError(QuotaReason::TotalBytes));
                }
                sv
            }
            StorageChangeOp::SetWithoutQuota(v) => v.to_string(),
            StorageChangeOp::Clear => unreachable!(),
        };

        log::trace!("saving data for '{}': writing", ext_id);
        tx.execute_cached(
            "INSERT INTO storage_sync_data(ext_id, data, sync_change_counter)
                VALUES (:ext_id, :data, 1)
                ON CONFLICT (ext_id) DO UPDATE
                set data=:data, sync_change_counter = sync_change_counter + 1",
            rusqlite::named_params! {
                ":ext_id": ext_id,
                ":data": &sval,
            },
        )?;
    }
    Ok(())
}

fn remove_from_db(tx: &Transaction<'_>, ext_id: &str) -> Result<()> {
    save_to_db(tx, ext_id, &StorageChangeOp::Clear)
}

// This is a "helper struct" for the callback part of the chrome.storage spec,
// but shaped in a way to make it more convenient from the rust side of the
// world.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StorageValueChange {
    #[serde(skip_serializing)]
    pub key: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub old_value: Option<JsonValue>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub new_value: Option<JsonValue>,
}

// This is, largely, a helper so that this serializes correctly as per the
// chrome.storage.sync spec. If not for custom serialization it should just
// be a plain vec
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct StorageChanges {
    pub changes: Vec<StorageValueChange>,
}

impl StorageChanges {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_capacity(n: usize) -> Self {
        Self {
            changes: Vec::with_capacity(n),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.changes.is_empty()
    }

    pub fn push(&mut self, change: StorageValueChange) {
        self.changes.push(change)
    }
}

// and it serializes as a map.
impl Serialize for StorageChanges {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
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

// A helper to determine the size of a key/value combination from the
// perspective of quota and getBytesInUse().
pub fn get_quota_size_of(key: &str, v: &JsonValue) -> usize {
    // Reading the chrome docs literally re the quota, the length of the key
    // is just the string len, but the value is the json val, as bytes.
    key.len() + v.to_string().len()
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
        if current.len() >= SYNC_MAX_ITEMS {
            return Err(Error::QuotaError(QuotaReason::MaxItems));
        }
        // Reading the chrome docs literally re the quota, the length of the key
        // is just the string len, but the value is the json val, as bytes
        if get_quota_size_of(&k, &v) > SYNC_QUOTA_BYTES_PER_ITEM {
            return Err(Error::QuotaError(QuotaReason::ItemBytes));
        }
        let change = StorageValueChange {
            key: k.clone(),
            old_value,
            new_value: Some(v.clone()),
        };
        changes.push(change);
        current.insert(k, v);
    }

    save_to_db(
        tx,
        ext_id,
        &StorageChangeOp::Set(JsonValue::Object(current)),
    )?;
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
    let mut existing = match (maybe_existing, keys.is_object()) {
        (None, true) => return Ok(keys),
        (None, false) => return Ok(JsonValue::Object(Map::new())),
        (Some(v), _) => v,
    };
    // take the quick path for null, where we just return the entire object.
    if keys.is_null() {
        return Ok(JsonValue::Object(existing));
    }
    // OK, so we need to build a list of keys to get.
    let keys_and_defaults = get_keys(keys);
    let mut result = Map::with_capacity(keys_and_defaults.len());
    for (key, maybe_default) in keys_and_defaults {
        if let Some(v) = existing.remove(&key) {
            result.insert(key, v);
        } else if let Some(def) = maybe_default {
            result.insert(key, def);
        }
        // else |keys| is a string/array instead of an object with defaults.
        // Don't include keys without default values.
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

    // Note: get_keys parses strings, arrays and objects, but remove()
    // is expected to only be passed a string or array of strings.
    let keys_and_defs = get_keys(keys);

    let mut result = StorageChanges::with_capacity(keys_and_defs.len());
    for (key, _) in keys_and_defs {
        if let Some(v) = existing.remove(&key) {
            result.push(StorageValueChange {
                key,
                old_value: Some(v),
                new_value: None,
            });
        }
    }
    if !result.is_empty() {
        save_to_db(
            tx,
            ext_id,
            &StorageChangeOp::SetWithoutQuota(JsonValue::Object(existing)),
        )?;
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
            old_value: Some(val),
        });
    }
    remove_from_db(tx, ext_id)?;
    Ok(result)
}

/// The implementation of `storage[.sync].getBytesInUse()`.
pub fn get_bytes_in_use(conn: &Connection, ext_id: &str, keys: JsonValue) -> Result<usize> {
    let maybe_existing = get_from_db(conn, ext_id)?;
    let existing = match maybe_existing {
        None => return Ok(0),
        Some(v) => v,
    };
    // Make an array of all the keys we we are going to count.
    let keys: Vec<&str> = match &keys {
        JsonValue::Null => existing.keys().map(|v| v.as_str()).collect(),
        JsonValue::String(name) => vec![name.as_str()],
        JsonValue::Array(names) => names.iter().filter_map(|v| v.as_str()).collect(),
        // in the spirit of json-based APIs, silently ignore strange things.
        _ => return Ok(0),
    };
    // We must use the same way of counting as our quota enforcement.
    let mut size = 0;
    for key in keys.into_iter() {
        if let Some(v) = existing.get(key) {
            size += get_quota_size_of(key, v);
        }
    }
    Ok(size)
}

/// Information about the usage of a single extension.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UsageInfo {
    /// The extension id.
    pub ext_id: String,
    /// The number of keys the extension uses.
    pub num_keys: usize,
    /// The number of bytes used by the extension. This result is somewhat rough
    /// -- it doesn't bother counting the size of the extension ID, or data in
    /// the mirror, and favors returning the exact number of bytes used by the
    /// column (that is, the size of the JSON object) rather than replicating
    /// the `get_bytes_in_use` return value for all keys.
    pub num_bytes: usize,
}

/// Exposes information about per-collection usage for the purpose of telemetry.
/// (Doesn't map to an actual `chrome.storage.sync` API).
pub fn usage(db: &Connection) -> Result<Vec<UsageInfo>> {
    type JsonObject = Map<String, JsonValue>;
    let sql = "
        SELECT ext_id, data
        FROM storage_sync_data
        WHERE data IS NOT NULL
        -- for tests and determinism
        ORDER BY ext_id
    ";
    db.query_rows_into(sql, [], |row| {
        let ext_id: String = row.get("ext_id")?;
        let data: String = row.get("data")?;
        let num_bytes = data.len();
        let num_keys = serde_json::from_str::<JsonObject>(&data)?.len();
        Ok(UsageInfo {
            ext_id,
            num_keys,
            num_bytes,
        })
    })
}

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
                old_value: Some(json!("old")),
                new_value: None,
            }],
        };
        assert_eq!(serde_json::to_string(&c)?, r#"{"key":{"oldValue":"old"}}"#);
        let c = StorageChanges {
            changes: vec![StorageValueChange {
                key: "key".to_string(),
                old_value: None,
                new_value: Some(json!({"foo": "bar"})),
            }],
        };
        assert_eq!(
            serde_json::to_string(&c)?,
            r#"{"key":{"newValue":{"foo":"bar"}}}"#
        );
        Ok(())
    }

    fn make_changes(changes: &[(&str, Option<JsonValue>, Option<JsonValue>)]) -> StorageChanges {
        let mut r = StorageChanges::with_capacity(changes.len());
        for (name, old_value, new_value) in changes {
            r.push(StorageValueChange {
                key: (*name).to_string(),
                old_value: old_value.clone(),
                new_value: new_value.clone(),
            });
        }
        r
    }

    #[test]
    fn test_simple() -> Result<()> {
        let ext_id = "x";
        let mut db = new_mem_db();
        let tx = db.transaction()?;

        // an empty store.
        for q in vec![JsonValue::Null, json!("foo"), json!(["foo"])].into_iter() {
            assert_eq!(get(&tx, ext_id, q)?, json!({}));
        }

        // Default values in an empty store.
        for q in vec![json!({ "foo": null }), json!({"foo": "default"})].into_iter() {
            assert_eq!(get(&tx, ext_id, q.clone())?, q.clone());
        }

        // Single item in the store.
        set(&tx, ext_id, json!({"foo": "bar" }))?;
        for q in vec![
            JsonValue::Null,
            json!("foo"),
            json!(["foo"]),
            json!({ "foo": null }),
            json!({"foo": "default"}),
        ]
        .into_iter()
        {
            assert_eq!(get(&tx, ext_id, q)?, json!({"foo": "bar" }));
        }

        // Default values in a non-empty store.
        for q in vec![
            json!({ "non_existing_key": null }),
            json!({"non_existing_key": 0}),
            json!({"non_existing_key": false}),
            json!({"non_existing_key": "default"}),
            json!({"non_existing_key": ["array"]}),
            json!({"non_existing_key": {"objectkey": "value"}}),
        ]
        .into_iter()
        {
            assert_eq!(get(&tx, ext_id, q.clone())?, q.clone());
        }

        // more complex stuff, including changes checking.
        assert_eq!(
            set(&tx, ext_id, json!({"foo": "new", "other": "also new" }))?,
            make_changes(&[
                ("foo", Some(json!("bar")), Some(json!("new"))),
                ("other", None, Some(json!("also new")))
            ])
        );
        assert_eq!(
            get(&tx, ext_id, JsonValue::Null)?,
            json!({"foo": "new", "other": "also new"})
        );
        assert_eq!(get(&tx, ext_id, json!("foo"))?, json!({"foo": "new"}));
        assert_eq!(
            get(&tx, ext_id, json!(["foo", "other"]))?,
            json!({"foo": "new", "other": "also new"})
        );
        assert_eq!(
            get(&tx, ext_id, json!({"foo": null, "default": "yo"}))?,
            json!({"foo": "new", "default": "yo"})
        );

        assert_eq!(
            remove(&tx, ext_id, json!("foo"))?,
            make_changes(&[("foo", Some(json!("new")), None)]),
        );

        assert_eq!(
            set(&tx, ext_id, json!({"foo": {"sub-object": "sub-value"}}))?,
            make_changes(&[("foo", None, Some(json!({"sub-object": "sub-value"}))),])
        );

        // XXX - other variants.

        assert_eq!(
            clear(&tx, ext_id)?,
            make_changes(&[
                ("foo", Some(json!({"sub-object": "sub-value"})), None),
                ("other", Some(json!("also new")), None),
            ]),
        );
        assert_eq!(get(&tx, ext_id, JsonValue::Null)?, json!({}));

        Ok(())
    }

    #[test]
    fn test_check_get_impl() -> Result<()> {
        // This is a port of checkGetImpl in test_ext_storage.js in Desktop.
        let ext_id = "x";
        let mut db = new_mem_db();
        let tx = db.transaction()?;

        let prop = "test-prop";
        let value = "test-value";

        set(&tx, ext_id, json!({ prop: value }))?;

        // this is the checkGetImpl part!
        let mut data = get(&tx, ext_id, json!(null))?;
        assert_eq!(value, json!(data[prop]), "null getter worked for {}", prop);

        data = get(&tx, ext_id, json!(prop))?;
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

        data = get(&tx, ext_id, json!([prop]))?;
        assert_eq!(value, json!(data[prop]), "array getter worked for {}", prop);
        assert_eq!(
            data.as_object().unwrap().len(),
            1,
            "array getter with a single key should return an object with a single property"
        );

        // checkGetImpl() uses `{ [prop]: undefined }` - but json!() can't do that :(
        // Hopefully it's just testing a simple object, so we use `{ prop: null }`
        data = get(&tx, ext_id, json!({ prop: null }))?;
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
        let mut db = new_mem_db();
        let tx = db.transaction()?;
        let ext_id = "xyz";

        set(&tx, ext_id, json!({"foo": "bar" }))?;

        assert_eq!(
            set(&tx, ext_id, json!({"foo": "bar" }))?,
            make_changes(&[("foo", Some(json!("bar")), Some(json!("bar")))]),
        );
        Ok(())
    }

    #[test]
    fn test_quota_maxitems() -> Result<()> {
        let mut db = new_mem_db();
        let tx = db.transaction()?;
        let ext_id = "xyz";
        for i in 1..SYNC_MAX_ITEMS + 1 {
            set(
                &tx,
                ext_id,
                json!({ format!("key-{}", i): format!("value-{}", i) }),
            )?;
        }
        let e = set(&tx, ext_id, json!({"another": "another"})).unwrap_err();
        match e {
            Error::QuotaError(QuotaReason::MaxItems) => {}
            _ => panic!("unexpected error type"),
        };
        Ok(())
    }

    #[test]
    fn test_quota_bytesperitem() -> Result<()> {
        let mut db = new_mem_db();
        let tx = db.transaction()?;
        let ext_id = "xyz";
        // A string 5 bytes less than the max. This should be counted as being
        // 3 bytes less than the max as the quotes are counted. Plus the length
        // of the key (no quotes) means we should come in 2 bytes under.
        let val = "x".repeat(SYNC_QUOTA_BYTES_PER_ITEM - 5);

        // Key length doesn't push it over.
        set(&tx, ext_id, json!({ "x": val }))?;
        assert_eq!(
            get_bytes_in_use(&tx, ext_id, json!("x"))?,
            SYNC_QUOTA_BYTES_PER_ITEM - 2
        );

        // Key length does push it over.
        let e = set(&tx, ext_id, json!({ "xxxx": val })).unwrap_err();
        match e {
            Error::QuotaError(QuotaReason::ItemBytes) => {}
            _ => panic!("unexpected error type"),
        };
        Ok(())
    }

    #[test]
    fn test_quota_bytes() -> Result<()> {
        let mut db = new_mem_db();
        let tx = db.transaction()?;
        let ext_id = "xyz";
        let val = "x".repeat(SYNC_QUOTA_BYTES + 1);

        // Init an over quota db with a single key.
        save_to_db(
            &tx,
            ext_id,
            &StorageChangeOp::SetWithoutQuota(json!({ "x": val })),
        )?;

        // Adding more data fails.
        let e = set(&tx, ext_id, json!({ "y": "newvalue" })).unwrap_err();
        match e {
            Error::QuotaError(QuotaReason::TotalBytes) => {}
            _ => panic!("unexpected error type"),
        };

        // Remove data does not fails.
        remove(&tx, ext_id, json!["x"])?;

        // Restore the over quota data.
        save_to_db(
            &tx,
            ext_id,
            &StorageChangeOp::SetWithoutQuota(json!({ "y": val })),
        )?;

        // Overwrite with less data does not fail.
        set(&tx, ext_id, json!({ "y": "lessdata" }))?;

        Ok(())
    }

    #[test]
    fn test_get_bytes_in_use() -> Result<()> {
        let mut db = new_mem_db();
        let tx = db.transaction()?;
        let ext_id = "xyz";

        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(null))?, 0);

        set(&tx, ext_id, json!({ "a": "a" }))?; // should be 4
        set(&tx, ext_id, json!({ "b": "bb" }))?; // should be 5
        set(&tx, ext_id, json!({ "c": "ccc" }))?; // should be 6
        set(&tx, ext_id, json!({ "n": 999_999 }))?; // should be 7

        assert_eq!(get_bytes_in_use(&tx, ext_id, json!("x"))?, 0);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!("a"))?, 4);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!("b"))?, 5);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!("c"))?, 6);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!("n"))?, 7);

        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(["a"]))?, 4);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(["a", "x"]))?, 4);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(["a", "b"]))?, 9);
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(["a", "c"]))?, 10);

        assert_eq!(
            get_bytes_in_use(&tx, ext_id, json!(["a", "b", "c", "n"]))?,
            22
        );
        assert_eq!(get_bytes_in_use(&tx, ext_id, json!(null))?, 22);
        Ok(())
    }

    #[test]
    fn test_usage() {
        let mut db = new_mem_db();
        let tx = db.transaction().unwrap();
        // '{"a":"a","b":"bb","c":"ccc","n":999999}': 39 bytes
        set(&tx, "xyz", json!({ "a": "a" })).unwrap();
        set(&tx, "xyz", json!({ "b": "bb" })).unwrap();
        set(&tx, "xyz", json!({ "c": "ccc" })).unwrap();
        set(&tx, "xyz", json!({ "n": 999_999 })).unwrap();

        // '{"a":"a"}': 9 bytes
        set(&tx, "abc", json!({ "a": "a" })).unwrap();

        tx.commit().unwrap();

        let usage = usage(&db).unwrap();
        let expect = [
            UsageInfo {
                ext_id: "abc".to_string(),
                num_keys: 1,
                num_bytes: 9,
            },
            UsageInfo {
                ext_id: "xyz".to_string(),
                num_keys: 4,
                num_bytes: 39,
            },
        ];
        assert_eq!(&usage, &expect);
    }
}
