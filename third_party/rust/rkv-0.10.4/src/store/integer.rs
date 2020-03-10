// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::marker::PhantomData;

use bincode::serialize;

use serde::Serialize;

use lmdb::Database;

use crate::error::{
    DataError,
    StoreError,
};

use crate::readwrite::{
    Readable,
    Writer,
};

use crate::value::Value;

use crate::store::single::SingleStore;

pub trait EncodableKey {
    fn to_bytes(&self) -> Result<Vec<u8>, DataError>;
}

pub trait PrimitiveInt: EncodableKey {}

impl PrimitiveInt for u32 {}

impl<T> EncodableKey for T
where
    T: Serialize,
{
    fn to_bytes(&self) -> Result<Vec<u8>, DataError> {
        serialize(self) // TODO: limited key length.
            .map_err(Into::into)
    }
}

pub(crate) struct Key<K> {
    bytes: Vec<u8>,
    phantom: PhantomData<K>,
}

impl<K> AsRef<[u8]> for Key<K>
where
    K: EncodableKey,
{
    fn as_ref(&self) -> &[u8] {
        self.bytes.as_ref()
    }
}

impl<K> Key<K>
where
    K: EncodableKey,
{
    #[allow(clippy::new_ret_no_self)]
    pub(crate) fn new(k: &K) -> Result<Key<K>, DataError> {
        Ok(Key {
            bytes: k.to_bytes()?,
            phantom: PhantomData,
        })
    }
}

pub struct IntegerStore<K>
where
    K: PrimitiveInt,
{
    inner: SingleStore,
    phantom: PhantomData<K>,
}

impl<K> IntegerStore<K>
where
    K: PrimitiveInt,
{
    pub(crate) fn new(db: Database) -> IntegerStore<K> {
        IntegerStore {
            inner: SingleStore::new(db),
            phantom: PhantomData,
        }
    }

    pub fn get<'env, T: Readable>(&self, reader: &'env T, k: K) -> Result<Option<Value<'env>>, StoreError> {
        self.inner.get(reader, Key::new(&k)?)
    }

    pub fn put(&self, writer: &mut Writer, k: K, v: &Value) -> Result<(), StoreError> {
        self.inner.put(writer, Key::new(&k)?, v)
    }

    pub fn delete(&self, writer: &mut Writer, k: K) -> Result<(), StoreError> {
        self.inner.delete(writer, Key::new(&k)?)
    }

    pub fn clear(&self, writer: &mut Writer) -> Result<(), StoreError> {
        self.inner.clear(writer)
    }
}

#[cfg(test)]
mod tests {
    use std::fs;
    use tempfile::Builder;

    use super::*;
    use crate::*;

    #[test]
    fn test_integer_keys() {
        let root = Builder::new().prefix("test_integer_keys").tempdir().expect("tempdir");
        fs::create_dir_all(root.path()).expect("dir created");
        let k = Rkv::new(root.path()).expect("new succeeded");
        let s = k.open_integer("s", StoreOptions::create()).expect("open");

        macro_rules! test_integer_keys {
            ($type:ty, $key:expr) => {{
                let mut writer = k.write().expect("writer");

                s.put(&mut writer, $key, &Value::Str("hello!")).expect("write");
                assert_eq!(s.get(&writer, $key).expect("read"), Some(Value::Str("hello!")));
                writer.commit().expect("committed");

                let reader = k.read().expect("reader");
                assert_eq!(s.get(&reader, $key).expect("read"), Some(Value::Str("hello!")));
            }};
        }

        test_integer_keys!(u32, std::u32::MIN);
        test_integer_keys!(u32, std::u32::MAX);
    }

    #[test]
    fn test_clear() {
        let root = Builder::new().prefix("test_integer_clear").tempdir().expect("tempdir");
        fs::create_dir_all(root.path()).expect("dir created");
        let k = Rkv::new(root.path()).expect("new succeeded");
        let s = k.open_integer("s", StoreOptions::create()).expect("open");

        {
            let mut writer = k.write().expect("writer");
            s.put(&mut writer, 1, &Value::Str("hello!")).expect("write");
            s.put(&mut writer, 2, &Value::Str("hello!")).expect("write");
            s.put(&mut writer, 3, &Value::Str("hello!")).expect("write");
            writer.commit().expect("committed");
        }

        {
            let mut writer = k.write().expect("writer");
            s.clear(&mut writer).expect("cleared");
            writer.commit().expect("committed");

            let reader = k.read().expect("reader");
            assert_eq!(s.get(&reader, 1).expect("read"), None);
            assert_eq!(s.get(&reader, 2).expect("read"), None);
            assert_eq!(s.get(&reader, 3).expect("read"), None);
        }
    }
}
